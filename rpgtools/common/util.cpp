#include "util.h"

#if defined OS_W32
#include <shellapi.h>
#elif defined OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <dirent.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <algorithm>
#include <cassert>

namespace Util
{
/* PRIVATE FUNCS */
static inline bool isDotOrDotDot(const unichar *str)
{
    if (str[0] == '.') {
        if (str[1] == 0)
            return true;
        if (str[1] == '.' && str[2] == 0)
            return true;
    }
    return false;
}

#if defined OS_W32

FILE *fopen(const std::string &str, const unichar *args)
{
    return _wfopen(W32::toWide(str).c_str(), const_cast<const WCHAR*>(args));
}

void mkdir(const std::string &dirname)
{
    CreateDirectoryW(W32::toWide(dirname).c_str(), NULL);
}

void mkdirsForFile(const std::string &filename)
{
    std::wstring tmp = W32::toWide(filename);
    for (unsigned int i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == '\\' || tmp[i] == '/') {
            tmp[i] = 0;
            CreateDirectoryW(tmp.c_str(), NULL);
            tmp[i] = '\\';
        }
    }
}

bool dirExists(const std::string &dirname)
{
    DWORD attrib = GetFileAttributesW(W32::toWide(dirname).c_str());
    return attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::string> listFiles(const std::string &path)
{
    assert(*path.rbegin() == PATH_SEPARATOR[0]);
    WIN32_FIND_DATAW fd;
    HANDLE hFind = NULL;

    std::vector<std::string> list;
    if ((hFind = FindFirstFileW(W32::toWide(path + "*").c_str(), &fd)) == INVALID_HANDLE_VALUE)
        throw std::runtime_error(path + ": could not list files");
    do {
        if (!isDotOrDotDot(fd.cFileName))
            list.push_back(W32::fromWide(fd.cFileName));
    } while(FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return list;
}

size_t getFileSize(const std::string &filename)
{
    //TODO getFileSize windows
    HANDLE hFile = CreateFileW(W32::toWide(filename).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);
    return static_cast<size_t>(size.QuadPart);
}

void deleteFile(const std::string &filename)
{
    DeleteFileW(W32::toWide(filename).c_str());
}

void deleteFolder(const std::string &filename)
{
    std::wstring wfilename = W32::toWide(filename);
    SHFILEOPSTRUCTW ops = {
        0,
        FO_DELETE,
        wfilename.c_str(),
        NULL,
        FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
        FALSE,
        NULL,
        NULL,
    };
    SHFileOperationW(&ops);
}

#elif defined OS_UNIX

FILE *fopen(const std::string &str, const unichar *ops)
{
    return ::fopen(str.c_str(), const_cast<const char*>(ops));
}

void mkdir(const std::string &filename)
{
    ::mkdir(filename.c_str(), 0777);
}

void mkdirsForFile(const std::string &filename)
{
    std::string tmp = filename;
    for (unsigned int i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            ::mkdir(tmp.c_str(), 0777);
            tmp[i] = '/';
        }
    }
}

bool dirExists(const std::string &dirname)
{
    struct stat st;
    return stat(dirname.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> listFiles(const std::string &path)
{
    assert(*path.rbegin() == PATH_SEPARATOR[0]);
    std::vector<std::string> list;
    DIR *dp = opendir(path.c_str());
    if (dp == NULL)
        throw std::runtime_error(path + ": could not list files");

    //Populate list
    dirent *ep;
    while ((ep = readdir(dp)) != NULL) {
        //TODO OS X UTF-8 conversion!
        if (!isDotOrDotDot(ep->d_name))
            list.push_back(ep->d_name);
    }
    closedir(dp);

    return list;
}

size_t getFileSize(const std::string &filename)
{
    struct stat st;
    stat(filename.c_str(), &st);
    return st.st_size;
}

void deleteFile(const std::string &filename)
{
    unlink(filename.c_str());
}

int deleteFolder_func(const char *filename, const struct stat *st, int flags, struct FTW *fwt)
{
    UNUSED(st);
    UNUSED(flags);
    UNUSED(fwt);
    return remove(filename);
}

void deleteFolder(const std::string &filename)
{
    nftw(filename.c_str(), deleteFolder_func, 64, FTW_DEPTH | FTW_PHYS);
}

#endif

std::string toLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string getExtension(const std::string &filename)
{
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos)
        return "";
    return Util::toLower(filename.substr(dot + 1));
}

std::string getWithoutExtension(const std::string &filename)
{
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos)
        return filename;
    return filename.substr(0, dot);
}

std::string readFileContents(const std::string &filename)
{
    size_t size = Util::getFileSize(filename);
    ifstream file(filename.c_str());
    std::vector<char> data(size);
    file.read(data.data(), size);
    return std::string(data.begin(), data.end());
}
}
