//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <radload/manager.hpp>
#include <string.h>
#include <radload/utility/hashtable.hpp>
#include <radload/utility/queue.hpp>
#include <radfile.hpp>

#ifdef RADLOAD_USE_WATCHER
#include <raddebugwatch.hpp>
#endif

radLoadManagerWrapper radLoad;

ILoadManager* ILoadManager::s_instance = NULL;

void radLoadInitialize( radLoadInit* init )
{
    if( !init )
    {
        radMemoryAllocator old = ::radMemorySetCurrentAllocator( RADMEMORY_ALLOC_TEMP );
        init = new radLoadInit();
        ::radMemorySetCurrentAllocator( old );
    }
    ILoadManager::s_instance = new radLoadManager( *init );
    delete init;
}

void radLoadTerminate()
{
    radLoad->Terminate();
    ILoadManager::s_instance = NULL;
}

void radLoadService()
{
    radLoad->Service();
}

ILoadManager* radLoadInstance()
{
    return ILoadManager::s_instance;
}

radLoadManager::radLoadManager( radLoadInit& init )
:
m_bSyncLoading( false ),
m_bDone( false ),
#ifdef RADLOAD_GATHER_STATS
m_totalLoads( 0 ),
m_completedLoads( 0 ),
m_pendingLoads( 0 ),
m_maxPendingLoads( 0 ),
m_minLoadTime( 0xFFFFFFFF ),
m_maxLoadTime( 0 ),
m_avgLoadTime( 0 ),
m_minQueuedTime( 0xFFFFFFFF ),
m_maxQueuedTime( 0 ),
m_avgQueuedTime( 0 ),
#endif
m_pCurrent( NULL ),
m_pFileLoaders( NULL ),
m_pDataLoaders( NULL ),
m_pLoadQueue( NULL ),
m_pThread( NULL ),
m_pMutex( NULL )
{
    // Make our hash tables so that they will never resize or repack
    m_pFileLoaders = new RefHashTable<radLoadFileLoader>( init.fileLoaderListSize, 200, init.fileLoaderListSize );
    m_pFileLoaders->AddRef();
    m_pDataLoaders = new RefHashTable<radLoadDataLoader>( init.dataLoaderListSize, 200, init.dataLoaderListSize );
    m_pDataLoaders->AddRef();
    m_pLoadQueue = new RefQueue<radLoadObject>( init.loadQueueSize );
    m_pLoadQueue->AddRef();
    m_pCallbacks = new RefQueue<radLoadCallback>( 32 );
    m_pCallbacks ->AddRef();

    ::radThreadCreateMutex( &m_pMutex );
    m_pMutex->Lock();
#ifdef RAD_IOS
    ::radThreadCreateSemaphore(&m_pLoaderTurn, 0);
    ::radThreadCreateSemaphore(&m_pMainTurn, 0);
#endif
    ::radThreadCreateThread( &m_pThread, radLoadManager::LoadThreadEntry, static_cast<void*>(this), IRadThread::PriorityNormal, init.loadThreadStackSize );

#ifdef RADLOAD_GATHER_STATS
#ifdef RADLOAD_USE_WATCHER
    radDbgWatchAddUnsignedInt( &m_totalLoads, "Total Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_completedLoads, "Completed Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_pendingLoads, "Pending Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_maxPendingLoads, "Max Pending Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_minLoadTime, "Min Load Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_maxLoadTime, "Max Load Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_avgLoadTime, "Average Load Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_minQueuedTime, "Min Queued Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_maxQueuedTime, "Max Queued Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_avgQueuedTime, "Average Queued Time", "radload\\stats", 0, 0 );
#endif
#endif
}

radLoadManager::~radLoadManager()
{
    // Destruction of objects happens in the Terminate function
}

void radLoadManager::AddCallback( radLoadCallback* callback )
{
    if( callback )
    {
        callback->AddRef();
        if( !IsLoadPending() )
        {
            callback->Done();
            callback->Release();
        }
        else
        {
            m_pLoadQueue->Push( callback );
        }
    }
}

void radLoadManager::AddDataLoader( radLoadDataLoader* dataLoader, radLoadClassID id )
{
    m_pDataLoaders->Store( id, dataLoader );
}

void radLoadManager::AddFileLoader( radLoadFileLoader* fileLoader, const char* extension )
{
    m_pFileLoaders->Store( radMakeCaseInsensitiveKey( extension ), fileLoader );
}

void radLoadManager::Cancel()
{
    unsigned int i = 0;

    while( !m_pLoadQueue->Empty() )
    {
       radLoadObject* obj = m_pLoadQueue->Pop();
       radLoadUpdatableRequest* req = dynamic_cast<radLoadUpdatableRequest*>( obj );
       if( req )
       {
           req->SetState( CANCELED );
       }
       obj->Release();
    }
    if( m_pCurrent )
    {
        m_pCurrent->Cancel();
    }
}

radLoadDataLoader* radLoadManager::GetDataLoader( radLoadClassID id )
{
    return m_pDataLoaders->Find( id );
}

radLoadFileLoader* radLoadManager::GetFileLoader( const char* extension )
{
    const char* ext = extension;
    if( *ext == '.' )
    {
        ext++;
    }
    return m_pFileLoaders->Find( radMakeCaseInsensitiveKey( ext ) );
}

void radLoadManager::InternalService()
{
#ifdef RAD_IOS
    m_pLoaderTurn->Wait();
#endif
    m_pMutex->Lock();
    while( !m_bDone )
    {
        if( !m_pLoadQueue->Empty() )
        {
            radLoadObject* obj = m_pLoadQueue->Pop();
            radLoadCallback* callback = dynamic_cast<radLoadCallback*>( obj );
            if( callback )
            {
                m_pCallbacks->Push(callback);
            }
            else
            {
                QueueItem* item = dynamic_cast<QueueItem*>(obj);
                if( item )
                {
                    m_pCurrent = item;
                    m_pCurrent->AddRef();
                    m_pCurrent->SetState( LOADING );

                    char* filename = item->GetOptions()->filename;

                    // Find the "." in filename.
                    // Review: RAD: There are lots of functions that do this!  Try:
                    /* 
                    char* extension = strrchr( filename, '.' );
                    rAssert( extension != NULL );
                    extension++;
                    */

                    // Find the file extension.
                    int i = strlen( filename ) - 1;
                    while( i && (filename[i] != '.') )
                    {
                        i--;
                    }
                    i++;

                    radLoadFileLoader* loader = m_pFileLoaders->Find( radMakeCaseInsensitiveKey( filename + i ) );
                    rAssert( loader );
                    radMemoryAllocator old = ::radMemorySetCurrentAllocator (item->GetOptions()->allocator);

                    loader->LoadFile( item->GetOptions(), static_cast<radLoadUpdatableRequest*>( item ) );
                    
                    ::radMemorySetCurrentAllocator (old);
                    if( m_pCurrent->GetState() == LOADING )
                    {
                        m_pCurrent->SetState( COMPLETE );
                    }
    #ifdef RADLOAD_GATHER_STATS
                    if( m_pCurrent->GetState() == COMPLETE )
                    {
                        m_completedLoads++;
                        unsigned int time = m_pCurrent->GetTotalLoadTime();
                        unsigned int queued = m_pCurrent->GetTotalQueuedTime();
                    
                        m_minLoadTime = (m_minLoadTime > time) ? time : m_minLoadTime;
                        m_maxLoadTime = (m_maxLoadTime < time) ? time : m_maxLoadTime;
                        m_avgLoadTime = (((m_avgLoadTime * (m_totalLoads - 1))/(m_totalLoads)) +
                                (time/m_totalLoads));

                        m_minQueuedTime = (m_minQueuedTime > queued) ? queued : m_minQueuedTime;
                        m_maxQueuedTime = (m_maxQueuedTime < queued) ? queued : m_maxQueuedTime;
                        m_avgQueuedTime = (((m_avgQueuedTime * (m_totalLoads - 1))/(m_totalLoads)) +
                                (queued/m_totalLoads));
                    }
                    m_pendingLoads--;
    #endif
                    radLoadObject::Release( m_pCurrent );
                }
            }
        }
        else
        {
            SwitchTasks();
        }
    }
    m_pMutex->Unlock();
}

bool radLoadManager::IsLoadPending()
{
    return (!m_pLoadQueue->Empty() || m_pCurrent);
}

bool radLoadManager::IsSyncLoading()
{
    return m_bSyncLoading;
}

void radLoadManager::Load( radLoadOptions* options, radLoadRequest** request )
{
#ifdef RADLOAD_GATHER_STATS
    m_totalLoads++;
    m_pendingLoads++;
    m_maxPendingLoads = (m_pendingLoads > m_maxPendingLoads) ? m_pendingLoads : m_maxPendingLoads;
#endif
    rAssert( options );
    rAssert( options->filename );
    options->syncLoad |= m_bSyncLoading;
    radMemoryAllocator old = ::radMemorySetCurrentAllocator( RADMEMORY_ALLOC_TEMP );
    QueueItem* item = new QueueItem( *options );
    ::radMemorySetCurrentAllocator( old );
    item->AddRef();
    m_pLoadQueue->Push( item );
    item->SetState( QUEUED );
    *request = static_cast<radLoadRequest*>(item);
    if( options->stream )
    {
        item->SetStream( options->stream );
    }
    if( options->syncLoad )
    {
        while( item->GetState() != COMPLETE )
        {
            SwitchTasks();
            radFileService();
        }
    }
}

void radLoadManager::Load( const char* filename, radLoadRequest** request )
{
    radLoadOptions options;
    options.filename = new char[strlen( filename )];
    strcpy( options.filename, filename );
    Load( &options, request );
}

unsigned int radLoadManager::LoadThreadEntry( void* data )
{
    radLoadManager* manager = static_cast<radLoadManager*>( data );
    manager->InternalService();
    return 0;
}

float radLoadManager::PercentDone()
{
    if( m_pLoadQueue->Empty() )
    {
        return 1.0f;
    }
    // While hardly the most accurate number out there, it'll work for now.
    return 1.0f / static_cast<float>(m_pLoadQueue->Size());
}

void radLoadManager::PrintStats()
{
#ifdef RADLOAD_GATHER_STATS
    rDebugPrintf( "RadLoad Stats\n" );
    rDebugPrintf( "=====================================\n" );
    rDebugPrintf( "\tTotal Loads : %d\n", m_totalLoads );
    rDebugPrintf( "\tCompleted Loads : %d\n", m_completedLoads );
    rDebugPrintf( "\tPending Loads : %d\n", m_pendingLoads );
    rDebugPrintf( "\tMax Pending Loads : %d\n", m_maxPendingLoads );
    rDebugPrintf( "\tMin Load Time : %d\n", m_minLoadTime );
    rDebugPrintf( "\tMax Load Time : %d\n", m_maxLoadTime );
    rDebugPrintf( "\tAverage Load Time : %d\n", m_avgLoadTime );
    rDebugPrintf( "\tMin Queued Time : %d\n", m_minQueuedTime );
    rDebugPrintf( "\tMax Queued Time : %d\n", m_maxQueuedTime );
    rDebugPrintf( "\tAverage Queued Time : %d\n", m_avgQueuedTime );
    rDebugPrintf( "=====================================\n" );
#endif
}

void radLoadManager::RemoveDataLoader( radLoadClassID id )
{
    radLoadDataLoader* loader = GetDataLoader( id );
    while( loader )
    {
        RemoveDataLoader( loader );
        loader = GetDataLoader( id );
    }
}

void radLoadManager::RemoveDataLoader( radLoadDataLoader* loader )
{
    m_pDataLoaders->Remove( loader );
}

void radLoadManager::RemoveFileLoader( const char* extension )
{
    radLoadFileLoader* loader = GetFileLoader( extension );
    while( loader )
    {
        RemoveFileLoader( loader );
        loader = GetFileLoader( extension );
    }
}

void radLoadManager::RemoveFileLoader( radLoadFileLoader* loader )
{
    m_pFileLoaders->Remove( loader );
}

void radLoadManager::Service()
{
    if( IsLoadPending() )
    {
        SwitchTasks();
    }

    if(!m_pCallbacks->Empty())
    {
        radLoadCallback* callback = m_pCallbacks->Pop();
        callback->Done();
        callback->Release();
    }


}

void radLoadManager::SetSyncLoading( bool sync )
{
    m_bSyncLoading = sync;
}

void radLoadManager::SwitchTasks()
{
#ifdef RAD_IOS
    // Sleep(0) is only a scheduler hint: the caller can immediately retake
    // the mutex, starving the loader for seconds on a busy physical phone.
    // Hand off exactly one cooperative slice while retaining serialized
    // engine/renderer access. File waits and parsing both yield through here.
    const bool loaderThread = radThreadGetActiveThread() == m_pThread;
    m_pMutex->Unlock();
    if (loaderThread)
    {
        m_pMainTurn->Signal();
        m_pLoaderTurn->Wait();
    }
    else
    {
        m_pLoaderTurn->Signal();
        m_pMainTurn->Wait();
    }
    m_pMutex->Lock();
#else
    m_pMutex->Unlock();
    radThreadSleep(0);
    m_pMutex->Lock();
#endif
}

void radLoadManager::Terminate()
{
    m_pFileLoaders->Release();
    m_pDataLoaders->Release();
    rAssert( m_pLoadQueue->Empty() );
    m_pLoadQueue->Release();
    m_pCallbacks->Release();
    m_bDone = true;
    m_pMutex->Unlock();
#ifdef RAD_IOS
    // Release the parked worker so it can observe termination and exit.
    m_pLoaderTurn->Signal();
#endif
    m_pThread->WaitForTermination();
    m_pThread->Release();
#ifdef RAD_IOS
    m_pLoaderTurn->Release();
    m_pMainTurn->Release();
#endif

    m_pMutex->Release();

    delete this;
}
    
