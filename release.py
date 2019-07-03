#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
#
#  Igel is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Igel is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Igel.  If not, see <http://www.gnu.org/licenses/>.
#

#!/usr/bin/python3

import os
import platform
import sys
import zipfile
import shutil

version = sys.argv[1]

def copy_and_overwrite(from_path, to_path):
    if os.path.exists(to_path):
        shutil.rmtree(to_path)
    shutil.copytree(from_path, to_path)
    
def zipdir(path, ziph):
    # ziph is zipfile handle
    for root, dirs, files in os.walk(path):
        for file in files:
            ziph.write(os.path.join(root, file))

if __name__ == '__main__':
    if platform.system() == 'Windows':
        shutil.rmtree('local', ignore_errors=True)
        shutil.rmtree('igel-' + version + '.bin', ignore_errors=True)
        os.system('del igel-' + version + '.bin.zip')
        os.system('mkdir local')
        os.system('mkdir local\\cmake64')
        os.chdir('local\\cmake64')
        os.system('cmake -G "Visual Studio 16 2019" -A x64 ..\..')
        os.system('cmake --build . --target ALL_BUILD --config Release')
        os.chdir('..\\..\\')
        os.system('mkdir igel-' + version + '.bin')
        os.system('mkdir igel-' + version + '.bin\\windows')
        os.system('copy local\\cmake64\\Release\\igel.exe igel-' + version + '.bin\\windows\\igel-x64_old_cpu.exe')
        shutil.rmtree('local')
        os.system('mkdir local')
        os.system('mkdir local\\cmake64')
        os.chdir('local\\cmake64')
        os.system('cmake -D_BTYPE=POPCNT -G "Visual Studio 16 2019" -A x64 ..\..')
        os.system('cmake --build . --target ALL_BUILD --config Release')
        os.chdir('..\\..\\')
        os.system('copy local\\cmake64\\Release\\igel.exe igel-' + version + '.bin\\windows\\igel-x64_popcnt.exe')
        os.system('copy history.txt igel-' + version + '.bin\\history.txt')
        zipf = zipfile.ZipFile('igel-' + version + '.bin.zip', 'w', zipfile.ZIP_DEFLATED)
        zipdir('igel-' + version + '.bin', zipf)
        zipf.close()
        shutil.rmtree('igel-' + version + '.bin')
    else:
        os.system('cmake .')
        os.system('make -j4')
