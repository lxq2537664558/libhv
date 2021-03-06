#include "FileCache.h"

#include "hscope.h"

#include "httpdef.h" // for http_content_type_str_by_suffix
#include "http_page.h" //make_index_of_page

file_cache_t* FileCache::Open(const char* filepath, void* ctx) {
    file_cache_t* fc = Get(filepath);
    bool modified = false;
    if (fc) {
        time_t tt;
        time(&tt);
        if (tt - fc->stat_time > file_stat_interval) {
            time_t mtime = fc->st.st_mtime;
            stat(filepath, &fc->st);
            fc->stat_time = tt;
            fc->stat_cnt++;
            if (mtime != fc->st.st_mtime) {
                modified = true;
                fc->stat_cnt = 1;
            }
        }
    }
    if (fc == NULL || modified) {
        int fd = open(filepath, O_RDONLY);
        if (fd < 0) {
            return NULL;
        }
        defer(close(fd);)
        if (fc == NULL) {
            struct stat st;
            fstat(fd, &st);
            if (S_ISREG(st.st_mode) ||
                (S_ISDIR(st.st_mode) &&
                 filepath[strlen(filepath)-1] == '/')) {
                fc = new file_cache_t;
                //fc->filepath = filepath;
                fc->st = st;
                time(&fc->open_time);
                fc->stat_time = fc->open_time;
                fc->stat_cnt = 1;
                cached_files[filepath] = fc;
            }
            else {
                return NULL;
            }
        }
        if (S_ISREG(fc->st.st_mode)) {
            // FILE
            fc->resize_buf(fc->st.st_size);
            read(fd, fc->filebuf.base, fc->filebuf.len);
            const char* suffix = strrchr(filepath, '.');
            if (suffix) {
                fc->content_type = http_content_type_str_by_suffix(++suffix);
            }
        }
        else if (S_ISDIR(fc->st.st_mode)) {
            // DIR
            std::string page;
            make_index_of_page(filepath, page, (const char*)ctx);
            fc->resize_buf(page.size());
            memcpy(fc->filebuf.base, page.c_str(), page.size());
            fc->content_type = http_content_type_str(TEXT_HTML);
        }
        time_t tt = fc->st.st_mtime;
        strftime(fc->last_modified, sizeof(fc->last_modified), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&tt));
        snprintf(fc->etag, sizeof(fc->etag), "\"%zx-%zx\"", fc->st.st_mtime, fc->st.st_size);
    }
    return fc;
}

int FileCache::Close(const char* filepath) {
    auto iter = cached_files.find(filepath);
    if (iter != cached_files.end()) {
        delete iter->second;
        iter = cached_files.erase(iter);
        return 0;
    }
    return -1;
}

file_cache_t* FileCache::Get(const char* filepath) {
    auto iter = cached_files.find(filepath);
    if (iter != cached_files.end()) {
        return iter->second;
    }
    return NULL;
}
