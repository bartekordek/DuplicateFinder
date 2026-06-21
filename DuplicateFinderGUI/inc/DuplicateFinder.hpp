#pragma once

#include "gameengine/IGameEngineApp.hpp"
#include "gameengine/Windowing/WinData.hpp"
#include "gameengine/Windowing/IWindow.hpp"
#include "gameengine/Input/MouseData.hpp"
#include <DuplicateFinderBase.hpp>

#include "CUL/Filesystem/FileDatabase.hpp"
#include "CUL/Filesystem/IFile.hpp"
#include "CUL/Filesystem/Path.hpp"
#include "CUL/Threading/ThreadWrap.hpp"
#include "CUL/Threading/Worker.hpp"
#include "CUL/Math/Rotation.hpp"

#include "CUL/STL_IMPORTS/STD_list.hpp"
#include "CUL/STL_IMPORTS/STD_deque.hpp"

namespace SDL2W
{
class IKey;
}

NAMESPACE_BEGIN( LOGLW )
class Camera;
class Cube;
class Sprite;
class TransformComponent;
class Quad;
NAMESPACE_END( LOGLW )

using String = LOGLW::String;
using FileSize = int64_t;
using MD5Value = String;
using Value = std::map<MD5Value, std::vector<CUL::FS::Path>>;

struct SMD5Group
{
    MD5Value md5;
    std::list<CUL::FS::Path> files;
};

struct SFileGroup
{
    FileSize fileSize = 0u;
    std::map<MD5Value, SMD5Group> MD5Group;
};

struct FileEntry
{
    String Path;
    CUL::Time ModTime;
};

struct SameFilesGroup
{
    std::uint64_t Size;
    MD5Value MD5;
    std::vector<FileEntry> Files;
    bool Skip{ false };

    bool operator==( const SameFilesGroup& arg ) const
    {
        return ( Size == arg.Size ) && ( MD5 == arg.MD5 );
    }

    bool operator<( const SameFilesGroup& arg ) const
    {
        return arg.Size < Size;
    }
};

class CApp final: public LOGLW::IGameEngineApp
{
public:
    CApp( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath );

    CApp( const CApp& ) = delete;
    CApp( CApp&& ) = delete;
    CApp& operator=( const CApp& ) = delete;
    CApp& operator=( CApp&& ) = delete;

    void addForCheckForDeletionList( const String& path );

    int callback( void* NotUsed, int argc, char** argv, char** azColName ) const;
    static CApp* s_instance;

    ~CApp();

protected:
private:
    void startDBLoad();
    void startFileSearch();
    void getList();

    struct FileDb
    {
        String size;
        String md5;
        String modTime;
        String path;
    };
    void setWorkersCount( uint8_t inCount );

    void onInit() override;
    void onWindowEvent( const LOGLW::WindowEvent::Type e ) override;
    void timerThread();
    void onKeyBoardEvent( const LOGLW::KeyboardState& key ) override;
    void customFrame() override;
    void onMouseEvent( const LOGLW::MouseData& md ) override;
    void guiIteration( float x, float y );
    void searchOneTime();
    void searchBackground();
    std::atomic_bool m_runBackground = false;

    void addFile( const String& path, int8_t workerId );
    void addFileToList( const String path );

    void addTask( const std::function<void( int8_t )>& task, const CUL::FS::Path& filePath ) const;
    String getModTimeFromDb( const String& filePath );
    void printCurrentMean();

    glm::vec3 moveOnSphere( float yaw, float pitch, float row, float rad );
    void addSearchDir();
    void removeDir();
    void addSkipDir();
    void removeSkipDir();

    void addExcludeWord( const String& inString );
    void removeLastExcludeWord();

    void chooseResultFile();
    void addDuplicate( const FileSize fileSize, const MD5Value& md5, const CUL::FS::Path& path );

    void startWorkers();
    void saveDuplicates();
    void fetchDuplicates();
    static std::set<String> getListOfMd5s( const std::vector<CUL::FS::FileInfo>& fiList );
    void searchAllFiles();
    void setMainStatus( const String& status );
    void showList();
    void getEarlisestFiles( std::vector<std::size_t>& outValue, const std::vector<FileEntry>& files );

    DuplicateFinderBase m_duplicateFinderBase;

    CUL::FS::Path m_outputFile;
    std::vector<CUL::FS::Path> m_skippedDirs;
    std::vector<String> m_excludeWords;

    std::atomic<bool> m_runTimer = true;
    CUL::ThreadWrapper m_thread;

    LOGLW::Camera* m_camera = nullptr;
    LOGLW::MouseData m_mouseData;

    std::unique_ptr<CUL::ITimer> m_timer;

    unsigned m_indices[4];
    unsigned m_bufferId = 0;
    float m_time = 0.0f;
    float m_angle = 0.f;
    float m_deg = 0.f;

    CUL::MATH::Rotation m_lookAngles;

    int m_mouseLastX = 0;
    int m_mouseLastY = 0;

    float m_yawLast = 0.f;
    float m_pitchLast = 0.f;

    glm::vec3 m_front = { 0.f, 0.f, 0.f };
    glm::vec3 m_right = { 0.f, 0.f, 0.f };
    float m_velocity = 0.2f;
    bool m_firstMouse = true;

    CUL::CULInterface* m_culInterface = nullptr;
    size_t m_maxThreadCount = 7;

    std::mutex m_duplicatesMtx;
    std::map<FileSize, SFileGroup> m_duplicates;

    std::atomic_bool m_workersEnabled = false;

    std::atomic<float> m_filesCount = 0.f;
    std::atomic<float> m_filesDone = 0.f;

    int m_last = 101;

    String m_empty;

    std::vector<String> m_deletionList;

    std::thread m_searchThread;

    String m_currentFileText;

    std::atomic<size_t> m_workersActive = 0;
    std::atomic<size_t> m_workerId = 0;

    bool m_searchStarted = false;

    std::int32_t m_minFileSizeMB{ 0 };
    std::int32_t m_maxFileSizeMB{ 81920 };

    bool m_initialDbFilesUpdated = false;
    CUL::FS::FileDatabase m_fileDb;
    static constexpr int64_t bytesINMegabyte = 1024 * 1024;

    std::mutex m_statusMutex;
    String m_statusText;
    std::atomic_bool m_loadingDb = false;

    bool m_continousSearch = true;
    bool m_run = true;

    std::uint8_t m_maxFileGroups{ 0u };
    mutable std::set<SameFilesGroup> m_fileGroups;
    std::mutex m_fileGroupsMtx;

    std::atomic<std::int64_t> m_foundFiles{ 0 };
    std::atomic<std::int64_t> m_processedFiles{ 0 };
    float m_percentage{ 0.f };
};