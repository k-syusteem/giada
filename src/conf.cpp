/* ---------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * conf
 *
 * ---------------------------------------------------------------------
 *
 * Copyright (C) 2010-2012 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * ------------------------------------------------------------------ */


#include "conf.h"


int Conf::openFileForReading() {

	char path[PATH_MAX];

#if defined(__linux__)
	snprintf(path, PATH_MAX, "%s/.giada/%s", getenv("HOME"), CONF_FILENAME);
#elif defined(_WIN32)
	snprintf(path, PATH_MAX, "%s", CONF_FILENAME);
#elif defined(__APPLE__)
	struct passwd *p = getpwuid(getuid());
	if (p == NULL) {
		puts("[Conf::openFile] unable to fetch user infos.");
		return 0;
	}
	else {
		const char *home = p->pw_dir;
		snprintf(path, PATH_MAX, "%s/Library/Application Support/Giada/giada.conf", home);
	}
#endif

	fp = fopen(path, "r");
	if (fp == NULL) {
		puts("[Conf::openFile] unable to open conf file for reading.");
		return 0;
	}
	return 1;
}


/* ------------------------------------------------------------------ */


int Conf::createConfigFolder(const char *path) {

	if (gDirExists(path))
		return 1;

	puts(".giada folder not present. Updating...");

	if (gMkdir(path)) {
		puts("status: ok");
		return 1;
	}
	else {
		puts("status: error!");
		return 0;
	}
}


/* ------------------------------------------------------------------ */


int Conf::openFileForWriting() {

	/* writing config file. In windows is in the same dir of the .exe,
	 * in Linux and OS X in home */

#if defined(__linux__)

	char giadaPath[PATH_MAX];
	sprintf(giadaPath, "%s/.giada", getenv("HOME"));

	if (!createConfigFolder(giadaPath))
		return 0;

	char path[PATH_MAX];
	sprintf(path, "%s/%s", giadaPath, CONF_FILENAME);

#elif defined(_WIN32)

	const char *path = CONF_FILENAME;

#elif defined(__APPLE__)

	struct passwd *p = getpwuid(getuid());
	const char *home = p->pw_dir;
	char giadaPath[PATH_MAX];
	snprintf(giadaPath, PATH_MAX, "%s/Library/Application Support/Giada", home);

	if (!createConfigFolder(giadaPath))
		return 0;

	char path[PATH_MAX];
	sprintf(path, "%s/%s", giadaPath, CONF_FILENAME);

#endif

	fp = fopen(path, "w");
	if (fp == NULL)
		return 0;
	return 1;

}


/* ------------------------------------------------------------------ */


void Conf::setDefault() {
	soundSystem    = DEFAULT_SOUNDSYS;
	soundDeviceOut = DEFAULT_SOUNDDEV_OUT;
	soundDeviceIn  = DEFAULT_SOUNDDEV_IN;
	samplerate     = DEFAULT_SAMPLERATE;
	buffersize     = DEFAULT_BUFSIZE;
	delayComp      = DEFAULT_DELAYCOMP;
	limitOutput    = false;

	pluginPath[0]  = '\0';
	patchPath [0]  = '\0';
	samplePath[0]  = '\0';

	recsStopOnChanHalt = false;
	chansStopOnSeqHalt = false;
	treatRecsAsLoops   = false;

	actionEditorZoom = 100;

	int arkeys[MAX_NUM_CHAN] = DEFAULT_KEY_ARRAY;
	for (int i=0; i<MAX_NUM_CHAN; i++)
		keys[i] = arkeys[i];
}



/* ------------------------------------------------------------------ */


int Conf::read() {

	if (!openFileForReading()) {
		puts("[Conf] unreadable .conf file, using default parameters");
		setDefault();
		return 0;
	}

	if (getValue("header") != "GIADACFG") {
		puts("[Conf] corrupted .conf file, using default parameters");
		setDefault();
		return -1;
	}

	soundSystem = atoi(getValue("soundSystem").c_str());
	if (!soundSystem & (SYS_API_ANY)) soundSystem = DEFAULT_SOUNDSYS;

	soundDeviceOut = atoi(getValue("soundDeviceOut").c_str());
	if (soundDeviceOut < 0) soundDeviceOut = DEFAULT_SOUNDDEV_OUT;

	soundDeviceIn = atoi(getValue("soundDeviceIn").c_str());
	if (soundDeviceIn < -1) soundDeviceIn = DEFAULT_SOUNDDEV_IN;

	channelsOut = atoi(getValue("channelsOut").c_str());
	channelsIn  = atoi(getValue("channelsIn").c_str());
	if (channelsOut < 0) channelsOut = 0;
	if (channelsIn < 0)  channelsIn  = 0;

	buffersize = atoi(getValue("buffersize").c_str());
	if (buffersize < 8) buffersize = DEFAULT_BUFSIZE;

	delayComp = atoi(getValue("delayComp").c_str());
	if (delayComp < 0) delayComp = DEFAULT_DELAYCOMP;

	browserX = atoi(getValue("browserX").c_str());
	browserY = atoi(getValue("browserY").c_str());
	browserW = atoi(getValue("browserW").c_str());
	browserH = atoi(getValue("browserH").c_str());
	if (browserX < 0) browserX = 0;
	if (browserY < 0) browserY = 0;
	if (browserW < 396) browserW = 396;
	if (browserH < 302) browserH = 302;

	actionEditorX    = atoi(getValue("actionEditorX").c_str());
	actionEditorY    = atoi(getValue("actionEditorY").c_str());
	actionEditorW    = atoi(getValue("actionEditorW").c_str());
	actionEditorH    = atoi(getValue("actionEditorH").c_str());
	actionEditorZoom = atoi(getValue("actionEditorZoom").c_str());
	if (actionEditorX < 0)      actionEditorX = 0;
	if (actionEditorY < 0)      actionEditorY = 0;
	if (actionEditorW < 640)    actionEditorW = 640;
	if (actionEditorH < 176)    actionEditorH = 176;
	if (actionEditorZoom < 100) actionEditorZoom = 100;

	sampleEditorX    = atoi(getValue("sampleEditorX").c_str());
	sampleEditorY    = atoi(getValue("sampleEditorY").c_str());
	sampleEditorW    = atoi(getValue("sampleEditorW").c_str());
	sampleEditorH    = atoi(getValue("sampleEditorH").c_str());
	if (sampleEditorX < 0)   sampleEditorX = 0;
	if (sampleEditorY < 0)   sampleEditorY = 0;
	if (sampleEditorW < 500) sampleEditorW = 500;
	if (sampleEditorH < 292) sampleEditorH = 292;

	configX = atoi(getValue("configX").c_str());
	configY = atoi(getValue("configY").c_str());
	if (configX < 0) configX = 0;
	if (configY < 0) configY = 0;

	pluginListX = atoi(getValue("pluginListX").c_str());
	pluginListY = atoi(getValue("pluginListY").c_str());
	if (pluginListX < 0) pluginListX = 0;
	if (pluginListY < 0) pluginListY = 0;

	bpmX = atoi(getValue("bpmX").c_str());
	bpmY = atoi(getValue("bpmY").c_str());
	if (bpmX < 0) bpmX = 0;
	if (bpmY < 0) bpmY = 0;

	beatsX = atoi(getValue("beatsX").c_str());
	beatsY = atoi(getValue("beatsY").c_str());
	if (beatsX < 0) beatsX = 0;
	if (beatsY < 0) beatsY = 0;

	aboutX = atoi(getValue("aboutX").c_str());
	aboutY = atoi(getValue("aboutY").c_str());
	if (aboutX < 0) aboutX = 0;
	if (aboutY < 0) aboutY = 0;

	samplerate = atoi(getValue("samplerate").c_str());
	if (samplerate < 8000) samplerate = DEFAULT_SAMPLERATE;

	limitOutput = atoi(getValue("limitOutput").c_str());

	std::string p = getValue("pluginPath");
	strncpy(pluginPath, p.c_str(), p.size());
	pluginPath[p.size()] = '\0';	// strncpy doesn't add '\0'

	p = getValue("patchPath");
	strncpy(patchPath, p.c_str(), p.size());
	patchPath[p.size()] = '\0';	// strncpy doesn't add '\0'

	p = getValue("samplePath");
	strncpy(samplePath, p.c_str(), p.size());
	samplePath[p.size()] = '\0';	// strncpy doesn't add '\0'

	for (unsigned i=0; i<MAX_NUM_CHAN; i++) {
		char tmp[16];
		sprintf(tmp, "keys%d", i);
		keys[i] = atoi(getValue(tmp).c_str());
	}

	recsStopOnChanHalt = atoi(getValue("recsStopOnChanHalt").c_str());
	chansStopOnSeqHalt = atoi(getValue("chansStopOnSeqHalt").c_str());
	treatRecsAsLoops   = atoi(getValue("treatRecsAsLoops").c_str());

	close();
	return 1;
}


/* ------------------------------------------------------------------ */


int Conf::write() {
	if (!openFileForWriting())
		return 0;

	fprintf(fp, "# --- Giada configuration file --- \n");
	fprintf(fp, "header=GIADACFG\n");
	fprintf(fp, "version=%s\n", VERSIONE);

	fprintf(fp, "soundSystem=%d\n",    soundSystem);
	fprintf(fp, "soundDeviceOut=%d\n", soundDeviceOut);
	fprintf(fp, "soundDeviceIn=%d\n",  soundDeviceIn);
	fprintf(fp, "channelsOut=%d\n",    channelsOut);
	fprintf(fp, "channelsIn=%d\n",     channelsIn);
	fprintf(fp, "buffersize=%d\n",     buffersize);
	fprintf(fp, "delayComp=%d\n",      delayComp);
	fprintf(fp, "samplerate=%d\n",     samplerate);
	fprintf(fp, "limitOutput=%d\n",    limitOutput);

	fprintf(fp, "pluginPath=%s\n", pluginPath);
	fprintf(fp, "patchPath=%s\n",  patchPath);
	fprintf(fp, "samplePath=%s\n", samplePath);

	fprintf(fp, "browserX=%d\n", browserX);
	fprintf(fp, "browserY=%d\n", browserY);
	fprintf(fp, "browserW=%d\n", browserW);
	fprintf(fp, "browserH=%d\n", browserH);

	fprintf(fp, "actionEditorX=%d\n",    actionEditorX);
	fprintf(fp, "actionEditorY=%d\n",    actionEditorY);
	fprintf(fp, "actionEditorW=%d\n",    actionEditorW);
	fprintf(fp, "actionEditorH=%d\n",    actionEditorH);
	fprintf(fp, "actionEditorZoom=%d\n", actionEditorZoom);

	fprintf(fp, "sampleEditorX=%d\n", sampleEditorX);
	fprintf(fp, "sampleEditorY=%d\n", sampleEditorY);
	fprintf(fp, "sampleEditorW=%d\n", sampleEditorW);
	fprintf(fp, "sampleEditorH=%d\n", sampleEditorH);

	fprintf(fp, "configX=%d\n", configX);
	fprintf(fp, "configY=%d\n", configY);

	fprintf(fp, "pluginListX=%d\n", pluginListX);
	fprintf(fp, "pluginListY=%d\n", pluginListY);

	fprintf(fp, "bpmX=%d\n", bpmX);
	fprintf(fp, "bpmY=%d\n", bpmY);

	fprintf(fp, "beatsX=%d\n", beatsX);
	fprintf(fp, "beatsY=%d\n", beatsY);

	fprintf(fp, "aboutX=%d\n", aboutX);
	fprintf(fp, "aboutY=%d\n", aboutY);

	for (unsigned i=0; i<MAX_NUM_CHAN; i++)
		fprintf(fp, "keys%d=%d\n", i, keys[i]);

	fprintf(fp, "recsStopOnChanHalt=%d\n", recsStopOnChanHalt);
	fprintf(fp, "chansStopOnSeqHalt=%d\n", chansStopOnSeqHalt);
	fprintf(fp, "treatRecsAsLoops=%d\n",   treatRecsAsLoops);

	close();
	return 1;
}



/* ------------------------------------------------------------------ */


void Conf::close() {
	if (fp != NULL)
		fclose(fp);
}


/* ------------------------------------------------------------------ */


void Conf::setPath(char *path, const char *p) {
	path[0] = '\0';
	strncpy(path, p, strlen(p));
	path[strlen(p)] = '\0';
}
