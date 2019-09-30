/* ***** BEGIN LICENSE BLOCK *****
 * (C)opyright 2008-2009 Aplix Corporation. anselm@aplixcorp.com
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <nspr.h>
#include <npapi.h>
#include <npruntime.h>
#include <npfunctions.h>

static NPObject        *so       = NULL;
static NPNetscapeFuncs *npnfuncs = NULL;
static NPP              inst     = NULL;

/* NPN */

static void logmsg(const char *msg) {
	FILE *out = fopen("npsimple.log", "a");
	if(out) {
		fputs(msg, out);
		fclose(out);
	}
}

static bool
hasMethod(NPObject* obj, NPIdentifier methodName) {
	logmsg("npsimple: hasMethod\n");
	return true;
}

static bool
invokeAddTab (NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	const NPString *id;
	id = &NPVARIANT_TO_STRING (args[0]);

	logmsg (id->UTF8Characters);
	logmsg ("\n");

	return true;
}

static bool
invoke(NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result) {
	int error;
	char *name;

	logmsg("npsimple: invoke\n");

	error = 1;
	name = npnfuncs->utf8fromidentifier(methodName);

	if(name && !strcmp (name, "addTab")) {
		logmsg("npsimple: invoke addTab\n");
		return invokeAddTab(obj, args, argCount, result);
	}

	npnfuncs->setexception(obj, "exception during invocation");
	return false;
}

static bool
hasProperty(NPObject *obj, NPIdentifier propertyName) {
	logmsg("npsimple: hasProperty\n");
	return false;
}

static bool
getProperty(NPObject *obj, NPIdentifier propertyName, NPVariant *result) {
	logmsg("npsimple: getProperty\n");
	return false;
}

static NPClass npcRefObject = {
	NP_CLASS_STRUCT_VERSION,
	NULL,
	NULL,
	NULL,
	hasMethod,
	invoke,
	invokeAddTab,
	hasProperty,
	getProperty,
	NULL,
	NULL,
};

/* NPP */

static NPError
nevv(NPMIMEType pluginType, NPP instance, short mode, short argc, char *argn[], char *argv[], NPSavedData *saved) {
	inst = instance;
	logmsg("npsimple: new\n");
	return NPERR_NO_ERROR;
}

static NPError
destroy(NPP instance, NPSavedData **save) {
	if(so)
		npnfuncs->releaseobject(so);
	so = NULL;
	logmsg("npsimple: destroy\n");
	return NPERR_NO_ERROR;
}

static NPError
getValue(NPP instance, NPPVariable variable, void *value) {
	inst = instance;
	switch(variable) {
	default:
		logmsg("npsimple: getvalue - default\n");
		return NPERR_GENERIC_ERROR;
	case NPPVpluginNameString:
		logmsg("npsimple: getvalue - name string\n");
		*((char **)value) = "WebFavPlugin";
		break;
	case NPPVpluginDescriptionString:
		logmsg("npsimple: getvalue - description string\n");
		*((char **)value) = "<a href=\"http://www.ubuntu.com/\">Canonical WebFav</a> plugin.";
		break;
	case NPPVpluginScriptableNPObject:
		logmsg("npsimple: getvalue - scriptable object\n");
		if(!so)
			so = npnfuncs->createobject(instance, &npcRefObject);
		npnfuncs->retainobject(so);
		*(NPObject **)value = so;
		break;
	case NPPVpluginNeedsXEmbed:
		logmsg("npsimple: getvalue - xembed\n");
		*((PRBool *)value) = PR_TRUE; /* for some reason returning false here results in the plugin getting shut down */
		break;
	}
	return NPERR_NO_ERROR;
}

static NPError /* expected by Safari on Darwin */
handleEvent(NPP instance, void *ev) {
	inst = instance;
	logmsg("npsimple: handleEvent\n");
	return NPERR_NO_ERROR;
}

static NPError /* expected by Opera */
setWindow(NPP instance, NPWindow* pNPWindow) {
	inst = instance;
	logmsg("npsimple: setWindow\n");
	return NPERR_NO_ERROR;
}

/* EXPORT */
#ifdef __cplusplus
extern "C" {
#endif

NPError OSCALL
NP_GetEntryPoints(NPPluginFuncs *nppfuncs) {
	logmsg("npsimple: NP_GetEntryPoints\n");
	nppfuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	nppfuncs->newp          = nevv;
	nppfuncs->destroy       = destroy;
	nppfuncs->getvalue      = getValue;
	nppfuncs->event         = handleEvent;
	nppfuncs->setwindow     = setWindow;

	return NPERR_NO_ERROR;
}

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32)(x)) & 0xff00) >> 8)
#endif

NPError OSCALL
NP_Initialize(NPNetscapeFuncs *npnf,
              NPPluginFuncs *nppfuncs)
{
	logmsg("npsimple: NP_Initialize\n");
	if(npnf == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if(HIBYTE(npnf->version) > NP_VERSION_MAJOR)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	npnfuncs = npnf;
	NP_GetEntryPoints(nppfuncs);
	return NPERR_NO_ERROR;
}

NPError
OSCALL NP_Shutdown() {
	logmsg("npsimple: NP_Shutdown\n");
	return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription(void) {
	logmsg("npsimple: NP_GetMIMEDescription\n");
	return "application/x-canonical-webfav:webfav:Canonical WebFav";
}

NPError OSCALL /* needs to be present for WebKit based browsers */
NP_GetValue(void *npp, NPPVariable variable, void *value) {
	inst = (NPP)npp;
	return getValue((NPP)npp, variable, value);
}

#ifdef __cplusplus
}
#endif
