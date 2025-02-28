﻿#pragma once

#if UE_BUILD_SHIPPING

#define FG_CVAR(Var, CVarName, VarRef)
#define FG_CVAR_DESC(Var, CVarName, CVarDesc, VarRef)
#define FG_CVAR_FLAG_DESC(Var, CVarName, VarRef)

#define FG_SLOG(Message, Color)
#define FG_SLOG_INFO(Message)
#define FG_SLOG_WARN(Message)
#deifne FG_SLOG_ERR(Message)

#else

#define FG_CVAR(Var, CVarName, VarRef) FAutoConsoleVariableRef Var(CVarName, VarRef, TEXT(""))
#define FG_CVAR_DESC(Var, CVarName, CVarDesc, VarRef) FAutoConsoleVariableRef Var(CVarName, VarRef, CVarDesc)
#define FG_CVAR_FLAG_DESC(Var, CVarName, VarRef) FG_CVAR_DESC(##Var, ##CVarName, TEXT("1: Enable, 0: Disable"), ##VarRef)

#define FG_SLOG(Message, Color) GEngine->AddOnScreenDebugMessage(-1, 1.f, ##Color, ##Message)
#define FG_SLOG_INFO(Message) FG_SLOG(##Message, FColor::White)
#define FG_SLOG_WARN(Message) FG_SLOG(##Message, FColor::Yellow)
#define FG_SLOG_ERR(Message) FG_SLOG(##Message, FColor::Red)

#endif
