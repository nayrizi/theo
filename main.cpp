/*
 *  Theo - A Mini File Archiver
 *
 *  Copyright (c) 2014 Muhammad Emara <m.a.emara@live.cm>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <list>
#include <string>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
typedef std::string String;
typedef std::list<fs::path> List;
typedef uint8_t byte;

List files;
std::vector<String> titles;

struct Stamp
{
    byte x = 0x52;
    byte y = 0x84;
    byte z = 0x91;
    uint32_t files_count;
} __attribute__((__packed__));


struct Entry
{
    uint32_t number;     // File order in crushed output
    uint32_t size;       // File size in bytes
    uint32_t nameLength; //strlen of filename plus the file extension
} __attribute__((__packed__));


void          usage();
int           init(const char* dir_name);
inline void   panic(const char* msg);
inline void   print_list();
void          crush(const char* out_file_name);
inline bool   check_stamp(const Stamp& stamp);
uint32_t      extract_files_count(const char* title);
void          extract(const char *title);


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        usage();
    }
    
    auto dir = ".";
    init(dir);
    
    extern char *optarg;
    extern int optind;
    char *filename = nullptr;
    int c, err = 0;
    bool cflag = false, xflag = false;
    
    while ( (c = getopt(argc, argv, "hc:x:")) != -1)
    {
        switch (c)
        {
            case 'h':
                usage();
                return 0;
            case 'c':
                cflag = true;
                filename = optarg;
                break;
            case 'x':
                xflag = true;
                filename = optarg;
                break;
            case '?':
                panic("Unknown argument!");
                break;
        }
    }
    
    if (cflag)
    {
        crush(filename);
    }
    
    if (xflag)
    {
        extract_files_count(filename);
        extract(filename);
    }

    return 0;
}

int init(const char* dir_name)
{
    fs::path dir(dir_name);
    
    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
        printf("%s is not a directory!", dir_name);
        exit(EXIT_FAILURE);
    }
    
    auto iter = fs::directory_iterator(dir);
    auto end  = fs::directory_iterator();
    
    while (iter != end)
    {
        if (!fs::is_regular_file(iter->path()))
        {
            iter++;
            continue;
        }
        
        files.push_back(iter->path());
        
        auto title = String(iter->path().stem().string() + 
                            iter->path().extension().string());
        titles.reserve(files.size());
        titles.push_back(title);
        iter++;
    }
    
    return 1;
}

inline void panic(const char* msg)
{
    printf("%s\n", msg);
    exit(EXIT_FAILURE);
}

inline bool check_stamp(const Stamp& stamp)
{
    if (stamp.x == 0x52 && stamp.y == 0x84 && stamp.z == 0x91)
    {
        return true;
    }
    
    return false;
}

inline void print_list()
{
    for (auto it = titles.begin(); it != titles.end(); it++)
    {
        printf("%s\n", it->c_str());
    }
}

uint32_t extract_files_count(const char* title)
{
    FILE* in = fopen(title, "r");
    
    if (in == nullptr)
    {
        printf("Couldn't access %s\n", title);
        exit(EXIT_FAILURE);
    }
    
    Stamp stamp;
    
    memset(&stamp, 0, sizeof(Stamp));
    fread(&stamp, sizeof(Stamp), 1, in);

    if (!check_stamp(stamp))
    {
        panic("Unknown file format!");
    }
    
    printf("Archive contains %d files\n", stamp.files_count);
}

void crush(const char* out_file_name)
{
    FILE *out = fopen(out_file_name, "wb");
    
    if (out == nullptr)
    {
        panic("Couldn't create %s!\nAborting...");
    }
    
    Stamp stamp;
    stamp.files_count = files.size();
    
    // stamp magic numbers
    fwrite(&stamp, sizeof(stamp), 1, out);
    
    printf("Archiving: \n");
    // append files
    for (auto i = 0; i < titles.size(); i++)
    {
        Entry entry;
        entry.number = i + 1;
        entry.size = fs::file_size(titles[i]);
        entry.nameLength = titles[i].length();
        
        // write entry to crushed output
        fwrite(&entry, sizeof(entry), 1, out);
        
        // write filename
        fwrite(titles[i].c_str(), titles[i].length(), 1, out);
        
        // open file and read contents
        
        byte *contents = new byte[entry.size + 1];
        FILE* in = fopen(titles[i].c_str(), "rb");
        
        if (in == nullptr)
        {
            printf("Couldn't access file: %s\n", titles[i].c_str());
            exit(EXIT_FAILURE);
        }
        
        printf("\t->%s\n", titles[i].c_str());
        
        for (auto j = 0; j < entry.size; j++)
        {
            fread(contents + j, sizeof(byte), 1, in);
            fwrite(contents + j, sizeof(byte), 1, out);
        }

        delete[] contents;
        fclose(in);
    }
    
    fclose(out);

}

void extract(const char *title)
{
    FILE* in = fopen(title, "rb");
    
    if (in == nullptr)
    {
        printf("Couldn't access %s\n", title);
        exit(EXIT_FAILURE);
    }
    
    Stamp stamp;
    
    memset(&stamp, 0, sizeof(Stamp));
    fread(&stamp, sizeof(Stamp), 1, in);

    if (!check_stamp(stamp))
    {
        panic("Unknown file format!");
    }
    
    printf("Extracting: ");
    for (auto i = 0; i < stamp.files_count; i++)
    {
        Entry entry;
        memset(&entry, 0, sizeof(Entry));
        
        fread(&entry, sizeof(Entry), 1, in);
        
        byte *title = new byte[entry.nameLength + 1];
        byte *contents = new byte[entry.size + 1];
        
        for (auto j = 0; j < entry.nameLength; j++)
        {
            fread(title + j, sizeof(byte), 1, in);
        }
        title[entry.nameLength] = '\0';
        
        printf("\t->%s\n", title);
        FILE *out = fopen(reinterpret_cast<const char *>(title), "wb");
        if (out == nullptr)
        {
            panic("Couldn't write extracted files!");
        }
        
        for (auto j = 0; j < entry.size; j++)
        {
            fread(contents + j, sizeof(byte), 1, in);
            fwrite(contents + j, sizeof(byte), 1, out);
        }
        contents[entry.size] = '\0';
        
        delete[] contents;
        delete[] title;
        fclose(out);
    }
    
    fclose(in);
}

void usage()
{
    printf("Theo 1.0 - Muhammad Emara <m.a.emara@live.com>\n");
    printf("A mini file archiver\n\n");
    printf("Usage:\n");
    printf("theo [option] [argument]\n");
    printf("  -c [output]       Crushing mode (Archiving)\n");
    printf("  -x [target]       Extraction mode\n");
    exit(EXIT_SUCCESS);
}