#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <cstdio>

#include "os.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define UNUSED(x) (void)(x)

#ifdef _MSC_VER
#define PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#else
#define PACK(decl) decl __attribute__((__packed__))
#endif

namespace Util
{
FILE *fopen(const std::string &str, const unichar *args);
void mkdir(const std::string &dirname);
void mkdirsForFile(const std::string &filename);
bool dirExists(const std::string &dirname);
std::vector<std::string> listFiles(const std::string &path);
std::string toLower(std::string str);
std::string getExtension(const std::string &filename);
std::string getWithoutExtension(const std::string &filename);
size_t getFileSize(const std::string &filename);
void deleteFile(const std::string &filename);
void deleteFolder(const std::string &filename);
std::string readFileContents(const std::string &filename);
}

#endif // UTIL_H
