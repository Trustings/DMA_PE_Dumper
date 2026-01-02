#pragma once
#define NOMINMAX
#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>
#endif
#include "memory.hpp"
#include <string>
#include <string_view>
#include <memory>
#include <fstream>
#include <mutex>
#include "vmmdll.h"
#include <iostream>
#include <vector>
#include <algorithm>
