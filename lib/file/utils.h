/*
    Copyright 2008 Brain Research Institute, Melbourne, Australia

    Written by J-Donald Tournier, 23/07/09.

    This file is part of MRtrix.

    MRtrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MRtrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __file_ops_h__
#define __file_ops_h__

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "debug.h"
#include "app.h"
#include "mrtrix.h"
#include "types.h"
#include "file/path.h"
#include "file/config.h"


namespace MR
{
  namespace File
  {

    namespace
    {
      inline char random_char ()
      {
        char c = rand () % 62;
        if (c < 10) return c+48;
        if (c < 36) return c+55;
        return c+61;
      }

      //CONF option: TmpFileDir
      //CONF default: `/tmp` (on Unix), `.` (on Windows)
      //CONF The prefix for temporary files (as used in pipelines). By default,
      //CONF these files get written to the current folder, which may cause
      //CONF performance issues when operating over distributed file systems. 
      //CONF In this case, it may be better to specify `/tmp/` here.
      const std::string& tmpfile_dir () {
        static const std::string __tmpfile_dir = File::Config::get ("TmpFileDir", 
#ifdef MRTRIX_WINDOWS
            "."
#else
            "/tmp"
#endif
            );
        return __tmpfile_dir;
      }

      //CONF option: TmpFilePrefix
      //CONF default: `mrtrix-tmp-`
      //CONF The prefix to use for the basename of temporary files. This will
      //CONF be used to generate a unique filename for the temporary file, by
      //CONF adding random characters to this prefix, followed by a suitable
      //CONF suffix (depending on file type). Note that this prefix can also be
      //CONF manipulated using the `MRTRIX_TMPFILE_PREFIX` environment
      //CONF variable, without editing the config file.
      const std::string __get_tmpfile_prefix () { 
        const char* from_env = getenv ("MRTRIX_TMPFILE_PREFIX");
        if (from_env) return from_env;
        return File::Config::get ("TmpFilePrefix", "mrtrix-tmp-"); 
      }

      const std::string& tmpfile_prefix () {
        static const std::string __tmpfile_prefix = __get_tmpfile_prefix();
        return __tmpfile_prefix;
      }

    }




    inline void create (const std::string& filename, int64_t size = 0)
    { 
      int fid = open (filename.c_str(), O_CREAT | O_RDWR | ( App::overwrite_files ? O_TRUNC : O_EXCL ), 0666);
      if (fid < 0) {
        if (App::check_overwrite_files_func && errno == EEXIST) 
          App::check_overwrite_files_func (filename);
        else 
          throw Exception ("output file \"" + filename + "\" already exists (use -force option to force overwrite)");
        fid = open (filename.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
      }
      if (fid < 0) {
        std::string mesg ("error creating file \"" + filename + "\": " + strerror (errno));
        if (errno == EEXIST) 
          mesg += " (use -force option to force overwrite)";
        throw Exception (mesg);
      }

      if (size) size = ftruncate (fid, size);
      close (fid);

      if (size) 
        throw Exception ("cannot resize file \"" + filename + "\": " + strerror (errno));
    }



    inline void resize (const std::string& filename, int64_t size)
    {
      DEBUG ("resizing file \"" + filename + "\" to " + str (size) + "...");

      int fd = open (filename.c_str(), O_RDWR, 0666);
      if (fd < 0)
        throw Exception ("error opening file \"" + filename + "\" for resizing: " + strerror (errno));
      int status = ftruncate (fd, size);
      close (fd);
      if (status)
        throw Exception ("cannot resize file \"" + filename + "\": " + strerror (errno));
    }




    inline bool is_tempfile (const std::string& name, const char* suffix = NULL)
    {
      if (Path::basename (name).compare (0, tmpfile_prefix().size(), tmpfile_prefix())) 
        return false;
      if (suffix) 
        if (!Path::has_suffix (name, suffix)) 
          return false;
      return true;
    }




    inline std::string create_tempfile (int64_t size = 0, const char* suffix = NULL)
    {
      DEBUG ("creating temporary file of size " + str (size));

      std::string filename (Path::join (tmpfile_dir(), tmpfile_prefix()) + "XXXXXX.");
      int rand_index = filename.size() - 7;
      if (suffix) filename += suffix;

      int fid;
      do {
        for (int n = 0; n < 6; n++)
          filename[rand_index+n] = random_char();
        fid = open (filename.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
      } while (fid < 0 && errno == EEXIST);

      if (fid < 0)
        throw Exception (std::string ("error creating temporary file in current working directory: ") + strerror (errno));




      int status = size ? ftruncate (fid, size) : 0;
      close (fid);
      if (status) throw Exception ("cannot resize file \"" + filename + "\": " + strerror (errno));
      return filename;
    }


    inline void mkdir (const std::string& folder) 
    {
      if (::mkdir (folder.c_str()
#ifndef MRTRIX_WINDOWS
            , 0777
#endif
            ))
        throw Exception ("error creating folder \"" + folder + "\": " + strerror (errno));
    }

    inline void unlink (const std::string& file) 
    {
      if (::unlink (file.c_str()))
        throw Exception ("error deleting file \"" + file + "\": " + strerror (errno));;
    }

    inline void rmdir (const std::string& folder, bool recursive = false)
    {
      if (recursive) {
        Path::Dir dir (folder);
        std::string entry;
        while ((entry = dir.read_name()).size()) {
          std::string path = Path::join (folder, entry);
          if (Path::is_dir (path))
            rmdir (path, true);
          else 
            unlink (path);
        }
      }
      DEBUG ("deleting folder \"" + folder + "\"...");
      if (::rmdir (folder.c_str()))
        throw Exception ("error deleting folder \"" + folder + "\": " + strerror (errno));
    }



  }
}

#endif

