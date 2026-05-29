cmake_minimum_required(VERSION 3.16)
project(ServoTerm VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Widgets SerialPort Network PrintSupport QUIET)
if(NOT Qt6_FOUND)
  find_package(Qt5 5.15 COMPONENTS Core Widgets SerialPort Network PrintSupport REQUIRED)
  set(QT_MAJOR 5)
else()
  set(QT_MAJOR 6)
endif()

set(SOURCES
  src/main.cpp
  src/MainWindow.cpp
  src/Actions.cpp
  src/SerialConnection.cpp
  src/ScopeDataDemux.cpp
  src/Oscilloscope.cpp
  src/XYOscilloscope.cpp
  src/HistoryLineEdit.cpp
  src/ConfigDialog.cpp
)

set(HEADERS
  src/MainWindow.h
  src/Actions.h
  src/SerialConnection.h
  src/ScopeDataDemux.h
  src/Oscilloscope.h
  src/XYOscilloscope.h
  src/HistoryLineEdit.h
  src/ConfigDialog.h
)

add_executable(servoterm ${SOURCES} ${HEADERS})

if(QT_MAJOR EQUAL 6)
  target_link_libraries(servoterm PRIVATE
    Qt6::Core Qt6::Widgets Qt6::SerialPort Qt6::Network Qt6::PrintSupport)
else()
  target_link_libraries(servoterm PRIVATE
    Qt5::Core Qt5::Widgets Qt5::SerialPort Qt5::Network Qt5::PrintSupport)
endif()

target_include_directories(servoterm PRIVATE src)
