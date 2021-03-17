#!/usr/bin/env bash

parallel 'echo {} | nc localhost 8088 -q 1' ::: {A..D}