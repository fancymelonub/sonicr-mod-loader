/**
 * Sonic R Mod Loader
 * Git version macros.
 */

#ifndef SONICRMODLOADER_GIT_H
#define SONICRMODLOADER_GIT_H

// git_version.h is generated by git_version.sh
#include "git_version.h"

// MODLOADER_GIT_VERSION: Macro for the git revision, if available.
#ifdef GIT_REPO
	#ifdef GIT_BRANCH
		#define MODLOADER_GIT_TMP_BRANCH GIT_BRANCH
		#ifdef GIT_SHAID
			#define MODLOADER_GIT_TMP_SHAID "/" GIT_SHAID
		#else /* !GIT_SHAID */
			#define MODLOADER_GIT_TMP_SHAID
		#endif /* GIT_SHAID */
	#else /* !GIT_BRANCH */
		#define MODLOADER_GIT_TMP_BRANCH
		#ifdef GIT_SHAID
			#define MODLOADER_GIT_TMP_SHAID GIT_SHAID
		#else /* !GIT_SHAID */
			#define MODLOADER_GIT_TMP_SHAID
		#endif /* GIT_SHAID */
	#endif /* GIT_BRANCH */
	
	#ifdef GIT_DIRTY
		#define MODLOADER_GIT_TMP_DIRTY "+"
	#else /* !GIT_DIRTY */
		#define MODLOADER_GIT_TMP_DIRTY
	#endif /* GIT_DIRTY */
	
	#define MODLOADER_GIT_VERSION "git: " MODLOADER_GIT_TMP_BRANCH MODLOADER_GIT_TMP_SHAID MODLOADER_GIT_TMP_DIRTY
	#ifdef GIT_DESCRIBE
		#define MODLOADER_GIT_DESCRIBE GIT_DESCRIBE MODLOADER_GIT_TMP_DIRTY
	#endif
#else /* !GIT_REPO */
	#ifdef MODLOADER_GIT_VERSION
		#undef MODLOADER_GIT_VERSION
	#endif /* MODLOADER_GIT_VERSION */
	#ifdef MODLOADER_GIT_DESCRIBE
		#undef MODLOADER_GIT_DESCRIBE
	#endif /* MODLOADER_GIT_DESCRIBE */
#endif /* GIT_REPO */

#endif /* __MODLOADER_GIT_H__ */
