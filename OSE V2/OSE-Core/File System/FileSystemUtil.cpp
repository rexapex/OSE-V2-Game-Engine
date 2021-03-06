#include "stdafx.h"
#include "FileSystemUtil.h"

#include <fstream>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShlObj_core.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace ose::fs
{
	// Loads the text file at 'path' and stores text in 'text'
	// @param {std::string &} path The relative? path of the file to load
	// @param {std::string &} text The string to be filled with the file's text
	void LoadTextFile(std::string const & path, std::string & text)
	{
		std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);

		if(in)
		{
			in.seekg(0, std::ios::end);
			text.resize(static_cast<size_t>(in.tellg()));
			in.seekg(0, std::ios::beg);
			in.read(&text[0], text.size());
			in.close();
		}
		else
		{
			throw std::invalid_argument("ERROR: could not open file, " + path);
		}
	}

	// Writes text file at 'path' with the contents 'text'
	// The file will be created if it does not already exist
	void WriteTextFile(std::string const & path, std::string const & text)
	{
		// https://stackoverflow.com/questions/478075/creating-files-in-c
		std::ofstream out(path);
		out << text << std::endl;
		out.close();
	}

	// Get the users home directory
	// Supports compile on Windows, Linux, (TODO MacOS) using ifdef
	// Returns Documents folder on Windows
	// WARNING: NOT THREAD SAFE
	void GetHomeDirectory(std::string & home_dir_path)
	{
	#if defined(__APPLE__) || defined(__linux__)
		//get linux/mac HOME environment variable
		char const * home_dir = getenv("HOME");
		if(home_dir == NULL)
		{
			std::cerr << "LINUX fs::GetHomeDirectory -> home_dir is NULL" << std::endl;
			//not thread safe
			home_dir = getpwuid(getuid()) != NULL ? getpwuid(getuid())->pw_dir : "/";
		}

		home_dir_path = std::string(home_dir);
	#elif defined(_WIN32)
		// Get windows Documents folder
		CHAR docs_path[MAX_PATH];
		HRESULT result = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, docs_path);

		if(result == S_OK)
		{
			home_dir_path = docs_path;
		}
		else
		{
			LOG_ERROR("Could not find HOME directory");
		}

		//get windows Documents folder
		//https://stackoverflow.com/questions/15916695/can-anyone-give-me-example-code-of-dupenv-s
		/*char * buffer;
		size_t size;
		if(_dupenv_s(&buffer, &size, "USERPROFILE") == 0 && buffer != nullptr)
		{
			std::string path(buffer);
			path += "\\Documents";
			home_dir_path = path;
			free(buffer);
		}*/
	#endif
	}

	// Get the directory of the executable
	std::string GetExecutableDirectory()
	{
#		ifdef _WIN32
			CHAR filename[MAX_PATH];
			DWORD size = GetModuleFileNameA(NULL, filename, MAX_PATH);
			if(size > 0)
			{
				return GetParentPath(filename);
			}
#		else
#			error fs::GetExecutableDirectory only supported on Windows
#		endif
		return "";
	}

	// Copy the file at the from path to the to path
	void CopyFile_(std::string const & from, std::string const & to)
	{
		try
		{
			// first, create the necessary directories
			std::filesystem::create_directories(GetParentPath(to));

			// then, copy the file
			bool success = std::filesystem::copy_file(from, to);

			if(success)
			{
				LOG("success: " + from + " copied successfully to " + to);
			}
			else
			{
				LOG("error: " + from + " could not be copied to " + to);
			}
		}
		catch(std::filesystem::filesystem_error &)
		{
			LOG("error: invalid arguments");
		}
	}

	// Creates the file at the path specified if it does not already exist
	// Returns true if the file is created, false if creation fails, false if file already exists
	bool CreateFile_(std::string const & path)
	{
		if(!DoesFileExist(path))
		{
			CreateDirs(path);
			std::ofstream ofs(path);
			ofs.close();
			return true;
		}
		return false;
	}

	// Creates directories given in path if they do not already exist
	// Returns true if the directories are created, false if some or all failed to create, false if all exist
	bool CreateDirs(std::string const & path)
	{
		auto & p = std::filesystem::path(path);
		if(p.has_filename())
		{
			return std::filesystem::create_directories(GetParentPath(path));
		}
		else
		{
			return std::filesystem::create_directories(path);
		}
	}

	// Returns true iff the path exists and is a file
	bool DoesFileExist(std::string const & path)
	{
		return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
	}

	// Get the filename of a path
	std::string GetFilenameFromPath(std::string const & path)
	{
		return std::filesystem::path(path).filename().string();
	}

	// Get the parent path of a path
	std::string GetParentPath(std::string const & path)
	{
		// TODO - Replace with rtrim function when implemented
		std::string s { path };
		s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
			return ch != '\\' && ch != '/';
		}).base(), s.end());
		return std::filesystem::path(s).parent_path().string();
	}

	// Get the relative path from a parent path and an absolute path
	std::string GetRelativePath(std::string const & abs_path, std::string const & parent_path)
	{
		return std::filesystem::relative(abs_path, parent_path).string();
	}
}
