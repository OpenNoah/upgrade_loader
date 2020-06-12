#!/bin/bash
TYPE=np1501 make clean
TYPE=np1501 make -j8
TYPE=np1380 make clean
TYPE=np1380 make -j8
