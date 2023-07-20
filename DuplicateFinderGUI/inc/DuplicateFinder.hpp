#pragma once

#include "gameengine/IGameEngineApp.hpp"

#include "SDL2Wrapper/WindowData.hpp"
#include "SDL2Wrapper/IWindow.hpp"
#include "SDL2Wrapper/Input/MouseData.hpp"

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


struct MD5Group
{
    MD5Value md5;
    std::list<CUL::FS::Path> files;
};

struct FileGroup
{
    FileSize fileSize = 0u;
    std::map<MD5Value, MD5Group> MD5Group;
};

class App final: public LOGLW::IGameEngineApp
{
public:
    App( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath );

    void addForCheckForDeletionList( const CUL::String& path );

    int callback( void* NotUsed, int argc, char** argv, char** azColName );
    static App* s_instance;

    ~App();

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


    void onInit() override;
    void onWindowEvent( const SDL2W::WindowEvent::Type e ) override;
    void timerThread();
    void onKeyBoardEvent( const SDL2W::KeyboardState& key ) override;
    void customFrame() override;
    void onMouseEvent( const SDL2W::MouseData& mouseData ) override;
    void updateEuler( float yaw, float pitch );
    void guiIteration();
    void searchOneTime();
    void searchBackground();
    std::atomic_bool m_runBackground = false;

    void addFile( const CUL::String& path, size_t workerId );
    void addFileToList( const CUL::String path );

    void addTask( std::function<void( size_t )> task );
    std::function<void(size_t)> getTask();
    void workerThreadMethod();
    unsigned getTasksLeft();
    CUL::String getModTimeFromDb( const CUL::String& filePath );
    void printCurrentMean();

    glm::vec3 moveOnSphere( float yaw, float pitch, float row, float rad );
    void addSearchDir();
    void removeDir();
    void chooseResultFile();
    void addDuplicate( const FileSize fileSize, const MD5Value& md5, const CUL::FS::Path& path );

    void startWorkers();
    void saveDuplicatesToFile();
    std::set<CUL::String> getListOfMd5s( const std::vector<CUL::FS::FileDatabase::FileInfo>& fiList );
    void setWorkerStatus( const CUL::String& status, size_t workerId );
    void searchAllFiles();
    void setMainStatus( const CUL::String& status );
    std::mutex m_workerStatusMtx;

    CUL::String m_outputFile;
    std::vector<CUL::String> m_searchPaths;

    std::atomic<bool> m_runTimer = true;
    CUL::ThreadWrapper m_thread;

    LOGLW::Camera* m_camera = nullptr;

    SDL2W::MouseData m_mouseData;

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
    std::vector<CUL::String> m_currentFiles;

    std::mutex m_duplicatesMtx;
    std::map<FileSize, FileGroup> m_duplicates;

    std::atomic_bool m_workersEnabled = false;

    std::mutex m_workersMtx;
    std::map<std::thread::id, std::thread> m_workers;

    std::mutex m_tasksMtx;
    std::vector<std::function<void(size_t)>> m_tasks;

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

    CUL::Worker m_updateDeletedFiles;
    CUL::Worker m_saveWorker;

    CUL::Worker m_genericWorker;

    std::mutex m_foundFileMtx;
    CUL::String m_foundFile;

    int m_minFileSizeBytes = 256;

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
};