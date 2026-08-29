/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QtCore>

#define ZDL_FLAG_NAMELESS    0x00001

#define ZDL_VERSION_STRING "3-2.1"
#define ZDL_DEV_BUILD 1
#define ZDL_PRIVATE_VERSION_STRING "3-2.1+spacebub"

#ifdef _WIN32
#define QFD_FILTER_DELIM    ";"
#define QFD_FILTER_ALL      "*.*"
#define QFD_QT_SEP(x)       QDir::fromNativeSeparators(x)
#else
#define QFD_FILTER_DELIM    " "
#define QFD_FILTER_ALL      "*"
#define QFD_QT_SEP(x)       x
#endif

#ifdef Q_PROCESSOR_X86_32
constexpr auto PROCESS = "32 bit";
#elif defined(Q_PROCESSOR_X86_64)
constexpr auto PROCESS = "64 bit";
#elif
constexpr auto PROCESS = "";
#endif

extern QDebug *zdlDebug;

#if defined(ZDL_BLACKBOX)
#include <QtCore>
#define LOGDATA() (*zdlDebug) << (QDateTime::currentDateTime().toString("[yyyy:MM:dd/hh:mm:ss.zzz]").append("@").append(__PRETTY_FUNCTION__).append("@").append(__FILE__).append(":").append(QString::number(__LINE__)).append("\t"))
#define LOGDATAO() (*zdlDebug) << (QDateTime::currentDateTime().toString("[yyyy:MM:dd/hh:mm:ss.zzz]").append("@").append(__PRETTY_FUNCTION__).append("@").append(__FILE__).append(":").append(QString::number(__LINE__)).append("#this=").append(DPTR(this)).append("\t"))

#if !defined(Q_WS_MAC)

#if UINTPTR_MAX == 0xffffffff
#ifndef _ZDL_NO_WARNINGS
#ifdef __GNUC__
#warning 32bit
#else
#pragma message("Warning: 32bit")
#endif
#endif
#define DPTR(ptr) QString("0x").append(QString("%1").arg((qulong)ptr, 0, 8, 16)
#else
#define DPTR(ptr) QString("0x").append(QString::number((qulonglong)ptr,16))
#endif

#else

#define DPTR(ptr) QString("0x").append("PTR")

#endif

#else
#define LOGDATA() (*zdlDebug)
#define LOGDATAO() (*zdlDebug)
#define DPTR(ptr) QString("")
#endif

#define LOCK_CLASS               QRecursiveMutex
#define LOCK_BUILDER()           new QRecursiveMutex()
#define GET_READLOCK(mlock)      (mlock)->lock()
#define RELEASE_READLOCK(mlock)  (mlock)->unlock()
#define GET_WRITELOCK(mlock)     (mlock)->lock()
#define RELEASE_WRITELOCK(mlock) (mlock)->unlock()
#define TRY_READLOCK(mlock, to)  (mlock)->tryLock(to)
#define TRY_WRITELOCK(mlock, to) (mlock)->tryLock(to)
