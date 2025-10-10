#pragma once

#include "gameengine/IGameEngineApp.hpp"
#include "gameengine/Windowing/WinData.hpp"
#include "gameengine/Windowing/IWindow.hpp"
#include "gameengine/Input/MouseData.hpp"

#include "CUL/Filesystem/FileDatabase.hpp"
#include "CUL/Filesystem/IFile.hpp"
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

using FileSize = int64_t;
using MD5Value = CUL::String;
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
    CUL::String Path;
    CUL::Time ModTime;
};

struct SameFilesGroup
{
    MD5Value MD5;
    CUL::String Size;
    std::vector<FileEntry> Files;
};

class CApp final: public LOGLW::IGameEngineApp
{
public:
    CApp( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath );

    CApp( const CApp& ) = delete;
    CApp( CApp&& ) = delete;
    CApp& operator=( const CApp& ) = delete;
    CApp& operator=( CApp&& ) = delete;

    void addForCheckForDeletionList( const CUL::String& path );

    int callback( void* NotUsed, int argc, char** argv, char** azColName );
    static CApp* s_instance;

    ~CApp();

protected:
private:
    void getList();

    struct FileDb
    {
        CUL::String size;
        CUL::String md5;
        CUL::String modTime;
        CUL::String path;
    };
    void setWorkersCount( uint8_t workersCount );

    void onInit() override;
    void onWindowEvent( const LOGLW::WindowEvent::Type e ) override;
    void timerThread();
    void onKeyBoardEvent( const LOGLW::KeyboardState& key ) override;
    void customFrame() override;
    void onMouseEvent( const LOGLW::MouseData& mouseData ) override;
    void guiIteration();
    void searchOneTime();
    void searchBackground();
    std::atomic_bool m_runBackground = false;

    void addFile( const CUL::String& path, int8_t workerId );
    void addFileToList( const CUL::String path );

    void addTask( std::function<void( int8_t )> task );
    CUL::String getModTimeFromDb( const CUL::String& filePath );
    void printCurrentMean();

    glm::vec3 moveOnSphere( float yaw, float pitch, float row, float rad );
    void addSearchDir();
    void removeDir();
    void chooseResultFile();
    void addDuplicate( const FileSize fileSize, const MD5Value& md5, const CUL::FS::Path& path );

    void startWorkers();
    void saveDuplicates();
    void fetchDuplicates();
    std::set<CUL::String> getListOfMd5s( const std::vector<CUL::FS::FileInfo>& fiList );
    void searchAllFiles();
    void setMainStatus( const CUL::String& status );
    void showList();

    CUL::FS::Path m_outputFile;
    std::vector<CUL::String> m_searchPaths;

    std::atomic<bool> m_runTimer = true;
    CUL::ThreadWrapper m_thread;

    LOGLW::Camera* m_camera = nullptr;

    LOGLW::MouseData m_mouseData;

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

    CUL::String m_empty;

    std::vector<CUL::String> m_deletionList;


    std::thread m_searchThread;

    CUL::String m_currentFileText;

    std::atomic<size_t> m_workersActive = 0;
    std::atomic<size_t> m_workerId = 0;

    bool m_searchStarted = false;

    std::uint64_t m_minFileSizeBytes{ 256u };

    bool m_initialDbFilesUpdated = false;
    CUL::FS::FileDatabase m_fileDb;
    static constexpr int64_t bytesINMegabyte = 1024 * 1024;

    std::mutex m_statusMutex;
    CUL::String m_statusText;
    std::atomic_bool m_loadingDb = false;

    size_t m_maxTasksInQueue = 64;

    std::atomic<int> m_filesTotalCount = 0;
    std::atomic<int> m_readFilesCount = 0;
    std::atomic<float> m_percentage = 0.f;

    bool m_continousSearch = true;
    bool m_run = true;


    std::uint8_t m_maxFileGroups{ 0u };
    std::vector<SameFilesGroup> m_fileGroups;
    std::mutex m_fileGroupsMtx;
};