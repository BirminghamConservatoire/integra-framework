//
//  util.cpp
//  UnitTests
//
//  Created by Jamie Bullock on 14/08/2018.
//  Copyright © 2018 Birmingham Conservatoire. All rights reserved.
//

#include "util.hpp"

#include <stdio.h>
#include <sys/types.h>
#include <string>
#include <dirent.h>

/*
// clang doesn't support std::filesystem yet...
// #include <filesystem>
std::size_t number_of_files_in_directory(std::filesystem::path path)
{
	using std::filesystem::directory_iterator;
	return std::distance(directory_iterator(path), directory_iterator{});
}
*/

int number_of_files_in_directory(const char *path)
{
    DIR *dp = opendir(path);
    int count = 0;
    struct dirent *ep;
    
    if (dp != NULL)
    {
        while ((ep = readdir(dp)))
        {
            if (ep->d_name[0] == '.')
            {
                continue;
            }
            ++count;
        }
        
        (void) closedir(dp);
    }
    else
    {
        perror("Couldn't open directory");
    }
    
    return count;
}

