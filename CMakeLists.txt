cmake_minimum_required(VERSION 3.16)
project(AXIS_Vehicules LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
  add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Network)

add_executable(AXIS_Vehicules
  main.cpp
  connexion.h connexion.cpp
  vehicule.h vehicule.cpp
  entretien.h entretien.cpp
  entretiendialog.h entretiendialog.cpp
  gaxis.h gaxis.cpp gaxis.ui
)
target_link_libraries(AXIS_Vehicules PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Network)
