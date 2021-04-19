// Copyright (c) 2021 Doug Binks
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "TaskScheduler_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enkiTaskScheduler*    pETS;


void PinnedTaskPretendIOFunc( void* pArgs_ )
{
    int32_t dataVal = *(int32_t*)pArgs_;
    printf("Run %d: Example PinnedTaskPretendIOFunc - this could perform network or file IO\n", dataVal );
}

int32_t g_Stop = 0; // we do not need an atomic as this is set by a pinned task on the thread it is read from

void PinnedTaskStopFunc( void* pArgs_ )
{
    g_Stop = 1; // call stop
}

void PinnedTaskRunPinnedTaskLoop( void* pArgs_ )
{
    uint32_t threadNumDesired = *(uint32_t*)pArgs_;
    uint32_t threadNum = enkiGetThreadNum( pETS );
    assert( threadNum == threadNumDesired );
    printf("PinnedTaskRunPinnedTaskLoop running on thread %d (should be thread %d)\n", threadNum, threadNumDesired );

    while( 0 == g_Stop )
    {
        enkiWaitForNewPinnedTasks( pETS );
        enkiRunPinnedTasks( pETS );
    }
}

int main(int argc, const char * argv[])
{
    struct enkiTaskSchedulerConfig config;
    enkiPinnedTask* pPinnedTaskRunPinnedTaskLoop;
    enkiPinnedTask* pPinnedTaskPretendIO;
    enkiPinnedTask* pPinnedTaskStop;

    pETS = enkiNewTaskScheduler();

    // get default config and request one external thread
    config = enkiGetTaskSchedulerConfig( pETS );
    config.numTaskThreadsToCreate += 1; // Create 1 extra thread for IO tasks (could create more if needed)
    enkiInitTaskSchedulerWithConfig( pETS, config );

    uint32_t threadNumIOTasks = config.numTaskThreadsToCreate; // thread 0 is this thread, so last thread is num threads created.

    // create task to run pinned task loop
    pPinnedTaskRunPinnedTaskLoop = enkiCreatePinnedTask( pETS, PinnedTaskRunPinnedTaskLoop, threadNumIOTasks );
    enkiAddPinnedTaskArgs( pETS, pPinnedTaskRunPinnedTaskLoop, &threadNumIOTasks );

    // send pretend IO commands to external thread
    pPinnedTaskPretendIO = enkiCreatePinnedTask( pETS, PinnedTaskPretendIOFunc, threadNumIOTasks );
    for( int32_t i=0; i<5; ++i )
    {
        // we re-use one task here as we are waiting for each to complete
        enkiAddPinnedTaskArgs( pETS, pPinnedTaskPretendIO, &i );

        // in most real world cases you would not wait for pinned IO task immediatly after
        // issueing it, but instead do work.
        // Rather than waiting can use dependencies or issue a pinned task to main thread (id 0) to send data
        enkiWaitForPinnedTask( pETS, pPinnedTaskPretendIO );
    }
    enkiDeletePinnedTask( pETS, pPinnedTaskPretendIO );

    // send stop to external thread
    pPinnedTaskStop = enkiCreatePinnedTask( pETS, PinnedTaskStopFunc, threadNumIOTasks );
    enkiAddPinnedTask( pETS, pPinnedTaskStop );

    // wait for task loop to complete
    enkiWaitForPinnedTask( pETS, pPinnedTaskRunPinnedTaskLoop );

    enkiDeletePinnedTask( pETS, pPinnedTaskStop );
    enkiDeletePinnedTask( pETS, pPinnedTaskRunPinnedTaskLoop );

    enkiDeleteTaskScheduler( pETS );
    printf("WaitForNewPinnedTasks_c.c completed\n" );

}
