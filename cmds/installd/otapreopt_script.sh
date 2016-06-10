#!/system/bin/sh

#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This script will run as a postinstall step to drive otapreopt.

STATUS_FD="$2"

# Maximum number of packages/steps.
MAXIMUM_PACKAGES=1000

PREPARE=$(cmd otadexopt prepare)
# Note: Ignore preparation failures. Step and done will fail and exit this.
#       This is necessary to support suspends - the OTA service will keep
#       the state around for us.

PROGRESS=$(cmd otadexopt progress)
print -u${STATUS_FD} "global_progress $PROGRESS"

i=0
while ((i<MAXIMUM_PACKAGES)) ; do
  cmd otadexopt step

  PROGRESS=$(cmd otadexopt progress)
  print -u${STATUS_FD} "global_progress $PROGRESS"

  DONE=$(cmd otadexopt done)
  if [ "$DONE" = "OTA incomplete." ] ; then
    sleep 1
    i=$((i+1))
    continue
  fi
  break
done

DONE=$(cmd otadexopt done)
if [ "$DONE" = "OTA incomplete." ] ; then
  echo "Incomplete."
else
  echo "Complete or error."
fi

print -u${STATUS_FD} "global_progress 1.0"
cmd otadexopt cleanup

exit 0
