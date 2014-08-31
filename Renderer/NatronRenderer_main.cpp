//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#include <csignal>

#if defined(Q_OS_UNIX)
#include <sys/signal.h>
#endif

#include <QCoreApplication>

#include "Engine/AppManager.h"

static void setShutDownSignal(int signalId);
static void handleShutDownSignal(int signalId);

int
main(int argc,
     char *argv[])
{

    bool isBackground;
    QString projectName,mainProcessServerName;
    QStringList writers;
    AppManager::parseCmdLineArgs(argc,argv,&isBackground,projectName,writers,mainProcessServerName);
    setShutDownSignal(SIGINT);   // shut down on ctrl-c
    setShutDownSignal(SIGTERM);   // shut down on killall
#if defined(Q_OS_UNIX)
    projectName = AppManager::qt_tildeExpansion(projectName);
#endif

    ///auto-background without a project name is not valid.
    if ( projectName.isEmpty() ) {
        AppManager::printUsage();

        return 1;
    }
    AppManager manager;
    if ( !manager.load(argc,argv,projectName,writers,mainProcessServerName) ) {
        AppManager::printUsage();

        return 1;
    } else {
        return 0;
    }

    return 0;
} //main

static void
setShutDownSignal(int signalId)
{
#if defined(Q_OS_UNIX)
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handleShutDownSignal;
    if (sigaction(signalId, &sa, NULL) == -1) {
        perror("setting up termination signal");
        exit(1);
    }
#else
    std::signal(signalId, handleShutDownSignal);
#endif
}

static void
handleShutDownSignal( int /*signalId*/ )
{
    QCoreApplication::exit(0);
}


