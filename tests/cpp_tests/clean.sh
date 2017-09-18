#!/bin/bash

find . ! -name "." ! -name "*.sh" ! -name "*.cpp" -exec rm {} \;
