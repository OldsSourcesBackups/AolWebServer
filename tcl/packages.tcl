# The contents of this file are subject to the AOLserver Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://aolserver.com/.
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is AOLserver Code and related documentation
# distributed by AOL.
#
# The Initial Developer of the Original Code is America Online,
# Inc. Portions created by AOL are Copyright (C) 1999 America Online,
# Inc. All Rights Reserved.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License (the "GPL"), in which case the
# provisions of GPL are applicable instead of those above.  If you wish
# to allow use of your version of this file only under the terms of the
# GPL and not to allow others to use your version of this file under the
# License, indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by the GPL.
# If you do not delete the provisions above, a recipient may use your
# version of this file under either the License or the GPL.
#
# $Header$

set section "ns/server/[ns_info server]/packages"

if {[llength [set libraryList [ns_config $section librarylist]]]} {
    foreach library $libraryList {
        if {[lsearch -exact $::auto_path $library] == -1} {
            lappend ::auto_path $library
        }
    }
}

if {![llength [set packageList [ns_config $section packagelist]]]} {
    return
}

foreach package $packageList { 
    if {[catch {set version [ns_ictl package require $package]}]} {
        ns_log error $::errorInfo
        continue
    }

    ns_log debug "Package Loaded: ${package}: ${version}"
}

proc _runPackageInit {packageList initCommand} {
    foreach package $packageList {
        set command "::${package}::${initCommand}"

        if {![llength [info commands $command]]} {
            continue
        }

        ns_log debug "Running: ${command}"

        if {[catch {eval $command}]} {
            ns_log error $::errorInfo
        }
    }
}

_runPackageInit $packageList nsInit 
_runPackageInit $packageList nsPostInit
rename _runPackageInit {}
