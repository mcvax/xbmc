/*
 *  Breakpad.cpp
 *  PlexMediaServer
 *
 *  Created by Max Feingold on 06/26/2013.
 *  Copyright 2013 Plex Inc. All rights reserved.
 *
 */

#include "Breakpad.h"
#include "filesystem/Directory.h"

#ifdef HAVE_BREAKPAD

//
// The crash dump filename convention is an id followed by -v- followed by a version string with a .dmp extension
// E.g. b1507f0c-82e4-42b2-81c8-4bf382c50ee0-v-0.9.8.1.dev-0b66fa4.dmp
//


#ifdef __linux__
#include <linux/limits.h>
#include <stdio.h>
#include <string>

/////////////////////////////////////////////////////////////////////////////////////////
static inline bool BreakPad_MinidumpCallback(const google_breakpad::MinidumpDescriptor& desc, void *context, bool succeeded)
{
  // Store the version in the filename.
  char finalPath[PATH_MAX+1];
  strcpy(finalPath, desc.path());
  finalPath[strlen(finalPath)-4] = '\0';
  strcat(finalPath, "-v-");
  strcat(finalPath, PLEX_MEDIA_SERVER_VERSION);
  strcat(finalPath, ".dmp");
  rename(desc.path(), finalPath);

  fprintf(stderr, "****** %s CRASHED, CRASH REPORT WRITTEN: %s\n", (const char*)context, finalPath);
  return succeeded;
}

#endif

#ifdef _WIN32

/////////////////////////////////////////////////////////////////////////////////////////
bool BreakPad_MinidumpCallback(const wchar_t* dump_path,
                               const wchar_t* minidump_id,
                               void* context,
                               EXCEPTION_POINTERS* exinfo,
                               MDRawAssertionInfo* assertion,
                               bool succeeded)
{
  if (dump_path && minidump_id)
  {
    // Rename the file, best effort
    wstring dumpPath = (wstring)dump_path + L"\\" + minidump_id + L".dmp";
    wstring newDumpPath = (wstring)dump_path + L"\\" + minidump_id + L"-v-" + utf8to16(PLEX_MEDIA_SERVER_VERSION) + L".dmp";
    MoveFileW(dumpPath.c_str(), newDumpPath.c_str());
  }
  return succeeded;
}
#endif

#ifdef __APPLE__
#include <string>
static inline bool BreakPad_MinidumpCallback(const char *dump_dir, const char *minidump_id, void *context, bool succeeded)
{
  // Store the version in the filename.
  std::string dp(dump_dir), mid(minidump_id);
  if (dp.empty() || mid.empty())
    return false;

  std::string dumpPath = dp + "/" + mid + ".dmp";
  std::string finalPath = dp + "/" + mid + "-v-" + PLEX_VERSION + ".dmp";
  fprintf(stderr, "***** moving %s to %s", dumpPath.c_str(), finalPath.c_str());
  rename(dumpPath.c_str(), finalPath.c_str());

  fprintf(stderr, "****** %s CRASHED, CRASH REPORT WRITTEN: %s\n", (const char*)context, finalPath.c_str());
  return succeeded;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////
BreakpadScope::BreakpadScope(const std::string& processName)
#ifdef __linux__
: m_processName(processName)
, m_descriptor(new google_breakpad::MinidumpDescriptor(GetPlexCrashPath()))
, m_eh(new google_breakpad::ExceptionHandler(*m_descriptor, NULL, BreakPad_MinidumpCallback, (void*)m_processName.c_str(), true, -1))
#endif
#ifdef _WIN32
: m_eh(new google_breakpad::ExceptionHandler(utf8to16(GetPlexCrashPath().c_str()).c_str(), NULL, BreakPad_MinidumpCallback, NULL, google_breakpad::ExceptionHandler::HANDLER_ALL))
#endif
#ifdef __APPLE__
: m_processName(processName)
, m_eh(new google_breakpad::ExceptionHandler(GetPlexCrashPath(), NULL, BreakPad_MinidumpCallback, (void*)m_processName.c_str(), true, NULL))
#endif
{
  fprintf(stderr, "Breakpad setup crashreports goes to %s\n", CSpecialProtocol::TranslatePath("special://temp/CrashReports").c_str());
  XFILE::CDirectory::Create("special://temp/CrashReports");
}

/////////////////////////////////////////////////////////////////////////////////////////
BreakpadScope::~BreakpadScope()
{
  delete m_eh;

#ifdef __linux__
  delete m_descriptor;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////
void BreakpadScope::Dump()
{
  // Generate a process dump on demand.
#ifdef __linux__
  m_eh->WriteMinidump(m_descriptor->directory(), BreakPad_MinidumpCallback, (void*)m_processName.c_str());
#endif

#if defined(_WIN32) || defined(__APPLE__)
  m_eh->WriteMinidump(m_eh->dump_path(), BreakPad_MinidumpCallback, NULL);
#endif
}

#endif
