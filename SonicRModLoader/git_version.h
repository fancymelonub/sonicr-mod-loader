/*
 * Basic versioning gathered from the git repository.
 * Automatically generated by ../git_version.sh.
 */

#ifndef GIT_VERSION_H
#define GIT_VERSION_H 1

/* whether this is a dist tarball or not */
#undef GIT_IS_DIST

/* No errors occured while running git */
#undef GIT_ERRORS

/* git utilities found */
#undef GIT_NOT_FOUND
#define GIT_VERSION "git version 2.14.1.windows.1"

/* The following helps debug why we sometimes do not find ".git/":
 * abs_repo_dir="" (should be "/path/to/.git")
 * abs_srcdir="/e/Documents/GitHub/SonicRModLoader/SonicRModLoader" (absolute top source dir "/path/to")
 * git_repo_dir="E:/Documents/GitHub/SonicRModLoader/.git" (usually ".git" or "/path/to/.git")
 * PWD="/e/Documents/GitHub/SonicRModLoader/SonicRModLoader"
 * srcdir="/e/Documents/GitHub/SonicRModLoader/SonicRModLoader"
 * working_dir="/e/Documents/GitHub/SonicRModLoader/SonicRModLoader"
 */

/* git repo found */
#define GIT_REPO 1

/* Git SHA ID of last commit */
#define GIT_SHAID "3cc57f61"

/* Branch this tree is on */
#define GIT_BRANCH "master"

/* git-describe: no description available (no tag?) */
#undef GIT_DESCRIBE

/* Local changes might be breaking things */
#define GIT_DIRTY 1

/* Define GIT_MESSAGE such that
 *    printf("%s: built from %s", argv[0], GIT_MESSAGE);
 * forms a proper sentence.
 */

#ifdef GIT_DIRTY
# define GIT_DIRTY_MSG " + changes"
#else /* !GIT_DIRTY */
# define GIT_DIRTY_MSG ""
#endif /* GIT_DIRTY */

#ifdef GIT_ERRORS
# define GIT_ERROR_MSG " with error: " GIT_ERRORS
#else /* !GIT_ERRORS */
# define GIT_ERROR_MSG ""
#endif /* GIT_ERRORS */

#ifdef GIT_IS_DIST
# define GIT_DIST_MSG "dist of "
#else /* !GIT_IS_DIST */
# define GIT_DIST_MSG ""
#endif /* GIT_IS_DIST */

#ifdef GIT_REPO
# ifdef GIT_NOT_FOUND
#  define GIT_MESSAGE GIT_DIST_MSG "git sources without git: " GIT_NOT_FOUND
# else /* !GIT_NOT_FOUND */
#  define GIT_MESSAGE \
       GIT_DIST_MSG \
       "git branch " GIT_BRANCH ", " \
       "commit " GIT_SHAID GIT_DIRTY_MSG \
       GIT_ERROR_MSG
# endif /* GIT_NOT_FOUND */
#else /* !GIT_REPO */
# define GIT_MESSAGE GIT_DIST_MSG "non-git sources" GIT_ERROR_MSG
#endif /* GIT_REPO */

#endif /* GIT_VERSION_H */
