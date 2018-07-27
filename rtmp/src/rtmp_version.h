#ifndef __RTMP_VERSION_H__
#define __RTMP_VERSION_H__


#define VERSION_MAJOR       3
#define VERSION_MINOR       0
#define VERSION_REVISION    19

static int snx_version_major(){
    return VERSION_MAJOR;
}

static int snx_version_minor(){
    return VERSION_MINOR;
}

static int snx_version_revision(){
    return VERSION_REVISION;
}

#endif
