/*
    Copyright (C) 2000-2003 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <stdint.h>

#include <cstdio> /* for sprintf */
#include <cstring>
#include <climits>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <regex.h>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "pbd/cpus.h"
#include "pbd/error.h"
#include "pbd/stacktrace.h"
#include "pbd/xml++.h"
#include "pbd/basename.h"
#include "pbd/strsplit.h"
#include "pbd/replace_all.h"

#include "ardour/utils.h"
#include "ardour/rc_configuration.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

string
legalize_for_path (const string& str)
{
	string::size_type pos;
	string illegal_chars = "/\\"; /* DOS, POSIX. Yes, we're going to ignore HFS */
	string legal;

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_of (illegal_chars, pos)) != string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return string (legal);
}

string
bump_name_once (const std::string& name, char delimiter)
{
	string::size_type delim;
	string newname;

	if ((delim = name.find_last_of (delimiter)) == string::npos) {
		newname  = name;
		newname += delimiter;
		newname += "1";
	} else {
		int isnumber = 1;
		const char *last_element = name.c_str() + delim + 1;
		for (size_t i = 0; i < strlen(last_element); i++) {
			if (!isdigit(last_element[i])) {
				isnumber = 0;
				break;
			}
		}

		errno = 0;
		int32_t version = strtol (name.c_str()+delim+1, (char **)NULL, 10);

		if (isnumber == 0 || errno != 0) {
			// last_element is not a number, or is too large
			newname  = name;
			newname  += delimiter;
			newname += "1";
		} else {
			char buf[32];

			snprintf (buf, sizeof(buf), "%d", version+1);

			newname  = name.substr (0, delim+1);
			newname += buf;
		}
	}

	return newname;

}

XMLNode *
find_named_node (const XMLNode& node, string name)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode* child;

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() == name) {
			return child;
		}
	}

	return 0;
}

int
cmp_nocase (const string& s, const string& s2)
{
	string::const_iterator p = s.begin();
	string::const_iterator p2 = s2.begin();

	while (p != s.end() && p2 != s2.end()) {
		if (toupper(*p) != toupper(*p2)) {
			return (toupper(*p) < toupper(*p2)) ? -1 : 1;
		}
		++p;
		++p2;
	}

	return (s2.size() == s.size()) ? 0 : (s.size() < s2.size()) ? -1 : 1;
}

int
touch_file (string path)
{
	int fd = open (path.c_str(), O_RDWR|O_CREAT, 0660);
	if (fd >= 0) {
		close (fd);
		return 0;
	}
	return 1;
}

string
region_name_from_path (string path, bool strip_channels, bool add_channel_suffix, uint32_t total, uint32_t this_one)
{
	path = PBD::basename_nosuffix (path);

	if (strip_channels) {

		/* remove any "?R", "?L" or "?[a-z]" channel identifier */

		string::size_type len = path.length();

		if (len > 3 && (path[len-2] == '%' || path[len-2] == '?' || path[len-2] == '.') &&
		    (path[len-1] == 'R' || path[len-1] == 'L' || (islower (path[len-1])))) {

			path = path.substr (0, path.length() - 2);
		}
	}

	if (add_channel_suffix) {

		path += '%';

		if (total > 2) {
			path += (char) ('a' + this_one);
		} else {
			path += (char) (this_one == 0 ? 'L' : 'R');
		}
	}

	return path;
}

bool
path_is_paired (string path, string& pair_base)
{
	string::size_type pos;

	/* remove any leading path */

	if ((pos = path.find_last_of (G_DIR_SEPARATOR)) != string::npos) {
		path = path.substr(pos+1);
	}

	/* remove filename suffixes etc. */

	if ((pos = path.find_last_of ('.')) != string::npos) {
		path = path.substr (0, pos);
	}

	string::size_type len = path.length();

	/* look for possible channel identifier: "?R", "%R", ".L" etc. */

	if (len > 3 && (path[len-2] == '%' || path[len-2] == '?' || path[len-2] == '.') &&
	    (path[len-1] == 'R' || path[len-1] == 'L' || (islower (path[len-1])))) {

		pair_base = path.substr (0, len-2);
		return true;

	}

	return false;
}

string
path_expand (string path)
{
        if (path.empty()) {
                return path;
        }

        /* tilde expansion */

        if (path[0] == '~') {
                if (path.length() == 1) {
                        return Glib::get_home_dir();
                }

                if (path[1] == '/') {
                        path.replace (0, 1, Glib::get_home_dir());
                } else {
                        /* can't handle ~roger, so just leave it */
                }
        }

	/* now do $VAR substitution, since wordexp isn't reliable */

	regex_t compiled_pattern;
	const int nmatches = 100;
	regmatch_t matches[nmatches];
	
	if (regcomp (&compiled_pattern, "\\$([a-zA-Z_][a-zA-Z0-9_]*|\\{[a-zA-Z_][a-zA-Z0-9_]*\\})", REG_EXTENDED)) {
                cerr << "bad regcomp\n";
                return path;
        }

	while (true) { 

		if (regexec (&compiled_pattern, path.c_str(), nmatches, matches, 0)) {
			break;
		}
		
		/* matches[0] gives the entire match */
		
		string match = path.substr (matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);
		
		/* try to get match from the environment */

                if (match[1] == '{') {
                        /* ${FOO} form */
                        match = match.substr (2, match.length() - 3);
                }

		char* matched_value = getenv (match.c_str());

		if (matched_value) {
			path.replace (matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so, matched_value);
		} else {
			path.replace (matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so, string());
                }

		/* go back and do it again with whatever remains after the
		 * substitution 
		 */
	}

	regfree (&compiled_pattern);

	/* canonicalize */

	char buf[PATH_MAX+1];

	if (realpath (path.c_str(), buf)) {
		return buf;
	} else {
		return string();
	}
}

string
search_path_expand (string path)
{
        if (path.empty()) {
                return path;
        }

	vector<string> s;
	vector<string> n;

	split (path, s, ':');

	for (vector<string>::iterator i = s.begin(); i != s.end(); ++i) {
		string exp = path_expand (*i);
		if (!exp.empty()) {
			n.push_back (exp);
		}
	}

	string r;

	for (vector<string>::iterator i = n.begin(); i != n.end(); ++i) {
		if (!r.empty()) {
			r += ':';
		}
		r += *i;
	}

	return r;
}

#if __APPLE__
string
CFStringRefToStdString(CFStringRef stringRef)
{
	CFIndex size =
		CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringRef) ,
		kCFStringEncodingUTF8);
	    char *buf = new char[size];

	std::string result;

	if(CFStringGetCString(stringRef, buf, size, kCFStringEncodingUTF8)) {
	    result = buf;
	}
	delete [] buf;
	return result;
}
#endif // __APPLE__

void
compute_equal_power_fades (framecnt_t nframes, float* in, float* out)
{
	double step;

	step = 1.0/(nframes-1);

	in[0] = 0.0f;

	for (framecnt_t i = 1; i < nframes - 1; ++i) {
		in[i] = in[i-1] + step;
	}

	in[nframes-1] = 1.0;

	const float pan_law_attenuation = -3.0f;
	const float scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	for (framecnt_t n = 0; n < nframes; ++n) {
		float inVal = in[n];
		float outVal = 1 - inVal;
		out[n] = outVal * (scale * outVal + 1.0f - scale);
		in[n] = inVal * (scale * inVal + 1.0f - scale);
	}
}

EditMode
string_to_edit_mode (string str)
{
	if (str == _("Splice")) {
		return Splice;
	} else if (str == _("Slide")) {
		return Slide;
	} else if (str == _("Lock")) {
		return Lock;
	}
	fatal << string_compose (_("programming error: unknown edit mode string \"%1\""), str) << endmsg;
	/*NOTREACHED*/
	return Slide;
}

const char*
edit_mode_to_string (EditMode mode)
{
	switch (mode) {
	case Slide:
		return _("Slide");

	case Lock:
		return _("Lock");

	default:
	case Splice:
		return _("Splice");
	}
}

SyncSource
string_to_sync_source (string str)
{
	if (str == _("MIDI Timecode") || str == _("MTC")) {
		return MTC;
	}

	if (str == _("MIDI Clock")) {
		return MIDIClock;
	}

	if (str == _("JACK")) {
		return JACK;
	}

	fatal << string_compose (_("programming error: unknown sync source string \"%1\""), str) << endmsg;
	/*NOTREACHED*/
	return JACK;
}

/** @param sh Return a short version of the string */
const char*
sync_source_to_string (SyncSource src, bool sh)
{
	switch (src) {
	case JACK:
		return _("JACK");

	case MTC:
		if (sh) {
			return _("MTC");
		} else {
			return _("MIDI Timecode");
		}

	case MIDIClock:
		return _("MIDI Clock");
	}
	/* GRRRR .... stupid, stupid gcc - you can't get here from there, all enum values are handled */
	return _("JACK");
}

float
meter_falloff_to_float (MeterFalloff falloff)
{
	switch (falloff) {
	case MeterFalloffOff:
		return METER_FALLOFF_OFF;
	case MeterFalloffSlowest:
		return METER_FALLOFF_SLOWEST;
	case MeterFalloffSlow:
		return METER_FALLOFF_SLOW;
	case MeterFalloffMedium:
		return METER_FALLOFF_MEDIUM;
	case MeterFalloffFast:
		return METER_FALLOFF_FAST;
	case MeterFalloffFaster:
		return METER_FALLOFF_FASTER;
	case MeterFalloffFastest:
		return METER_FALLOFF_FASTEST;
	default:
		return METER_FALLOFF_FAST;
	}
}

MeterFalloff
meter_falloff_from_float (float val)
{
	if (val == METER_FALLOFF_OFF) {
		return MeterFalloffOff;
	}
	else if (val <= METER_FALLOFF_SLOWEST) {
		return MeterFalloffSlowest;
	}
	else if (val <= METER_FALLOFF_SLOW) {
		return MeterFalloffSlow;
	}
	else if (val <= METER_FALLOFF_MEDIUM) {
		return MeterFalloffMedium;
	}
	else if (val <= METER_FALLOFF_FAST) {
		return MeterFalloffFast;
	}
	else if (val <= METER_FALLOFF_FASTER) {
		return MeterFalloffFaster;
	}
	else {
		return MeterFalloffFastest;
	}
}

AutoState
ARDOUR::string_to_auto_state (std::string str)
{
	if (str == X_("Off")) {
		return Off;
	} else if (str == X_("Play")) {
		return Play;
	} else if (str == X_("Write")) {
		return Write;
	} else if (str == X_("Touch")) {
		return Touch;
	}

	fatal << string_compose (_("programming error: %1 %2"), "illegal AutoState string: ", str) << endmsg;
	/*NOTREACHED*/
	return Touch;
}

string
ARDOUR::auto_state_to_string (AutoState as)
{
	/* to be used only for XML serialization, no i18n done */

	switch (as) {
	case Off:
		return X_("Off");
		break;
	case Play:
		return X_("Play");
		break;
	case Write:
		return X_("Write");
		break;
	case Touch:
		return X_("Touch");
	}

	fatal << string_compose (_("programming error: %1 %2"), "illegal AutoState type: ", as) << endmsg;
	/*NOTREACHED*/
	return "";
}

AutoStyle
ARDOUR::string_to_auto_style (std::string str)
{
	if (str == X_("Absolute")) {
		return Absolute;
	} else if (str == X_("Trim")) {
		return Trim;
	}

	fatal << string_compose (_("programming error: %1 %2"), "illegal AutoStyle string: ", str) << endmsg;
	/*NOTREACHED*/
	return Trim;
}

string
ARDOUR::auto_style_to_string (AutoStyle as)
{
	/* to be used only for XML serialization, no i18n done */

	switch (as) {
	case Absolute:
		return X_("Absolute");
		break;
	case Trim:
		return X_("Trim");
		break;
	}

	fatal << string_compose (_("programming error: %1 %2"), "illegal AutoStyle type: ", as) << endmsg;
	/*NOTREACHED*/
	return "";
}

std::string
bool_as_string (bool yn)
{
	return (yn ? "yes" : "no");
}

const char*
native_header_format_extension (HeaderFormat hf, const DataType& type)
{
        if (type == DataType::MIDI) {
                return ".mid";
        }

        switch (hf) {
        case BWF:
                return ".wav";
        case WAVE:
                return ".wav";
        case WAVE64:
                return ".w64";
        case CAF:
                return ".caf";
        case AIFF:
                return ".aif";
        case iXML:
                return ".ixml";
        case RF64:
                return ".rf64";
        }

        fatal << string_compose (_("programming error: unknown native header format: %1"), hf);
        /*NOTREACHED*/
        return ".wav";
}

bool
matching_unsuffixed_filename_exists_in (const string& dir, const string& path)
{
        string bws = basename_nosuffix (path);
	struct dirent* dentry;
	struct stat statbuf;
	DIR* dead;
        bool ret = false;

        if ((dead = ::opendir (dir.c_str())) == 0) {
                error << string_compose (_("cannot open directory %1 (%2)"), dir, strerror (errno)) << endl;
                return false;
        }

        while ((dentry = ::readdir (dead)) != 0) {

                /* avoid '.' and '..' */

                if ((dentry->d_name[0] == '.' && dentry->d_name[1] == '\0') ||
                    (dentry->d_name[2] == '\0' && dentry->d_name[0] == '.' && dentry->d_name[1] == '.')) {
                        continue;
                }

                string fullpath = Glib::build_filename (dir, dentry->d_name);

                if (::stat (fullpath.c_str(), &statbuf)) {
                        continue;
                }

                if (!S_ISREG (statbuf.st_mode)) {
                        continue;
                }

                string bws2 = basename_nosuffix (dentry->d_name);

                if (bws2 == bws) {
                        ret = true;
                        break;
                }
        }

        ::closedir (dead);
        return ret;
}

uint32_t
how_many_dsp_threads ()
{
        /* CALLER MUST HOLD PROCESS LOCK */

        int num_cpu = hardware_concurrency();
        int pu = Config->get_processor_usage ();
        uint32_t num_threads = max (num_cpu - 1, 2); // default to number of cpus minus one, or 2, whichever is larger

        if (pu < 0) {
                /* pu is negative: use "pu" less cores for DSP than appear to be available
                 */

                if (-pu < num_cpu) {
                        num_threads = num_cpu + pu;
                }

        } else if (pu == 0) {

                /* use all available CPUs
                 */

                num_threads = num_cpu;

        } else {
                /* use "pu" cores, if available
                 */

                num_threads = min (num_cpu, pu);
        }

        return num_threads;
}

double gain_to_slider_position_with_max (double g, double max_gain)
{
        return gain_to_slider_position (g * 2.0/max_gain);
}

double slider_position_to_gain_with_max (double g, double max_gain)
{
	return slider_position_to_gain (g * max_gain/2.0);
}

extern "C" {
	void c_stacktrace() { stacktrace (cerr); }
}
