#
# $Header$
#

package require Tcl 8.4
package provide make 1.0

set INSTALL_DIR "./installed"
set TCL_DIR "../tcl_core-8-4-6/win/installed"

namespace eval ::make {
    proc flag {key args} {
        variable flags
        if {[llength $args]} {
            set flags($key) [lindex $args 0]
        }
        if {[info exists flags($key)]} {
            return $flags($key)
        }
    }

    proc depends {output inputs} {
        variable depends
        foreach input $inputs {
            lappend depends($output) $input
        }
    }        

    proc rule {pattern script} {
        variable rules
        lappend rules($pattern) $script
    }

    proc execute_rules {target} {
        variable rules
        variable depends
        set inputs ""
        if {[info exists depends($target)]} {
            set inputs $depends($target)
        }
        foreach {pattern scripts} [array get rules] {
            if {[string match $pattern $target]} {
                foreach script $scripts {
                    if {[catch {eval $script $target $inputs} err] == 1} {
                        puts "$target failed: $err"
                        return 1
                    }
                }
            }
        }
        return 0
    }

    proc build {target} {
        if {![string length $target]} {
            return [build all]
        }
        variable depends
        if {[info exists depends($target)]} {
            foreach input $depends($target) {
                if {[build $input]} {
                    return 1
                }
            }
        }
        if {[execute_rules $target]} {
            return 1
        }
        return 0
    }

    proc newer {target depends} {
        set time 0
        if {[file exists $target]} {
            set time [file mtime $target]
        }
        foreach input $depends {
            if {$time < [file mtime $input]} {
                return 0
            }
        }
        return 1
    }

    proc replace {string from to} {
        regsub -all -- "(?q)$from" $string $to string
        return $string
    }
    
    proc filter {list regexp} {
        set new [list]
        foreach element $list {
            if {![regexp -- $regexp $element]} {
                lappend new $element
            }
        }
        return $new
    }

    proc compile_obj {target args} {
        variable depends
        set base [file rootname $target]
        set inputs $base.c
        foreach {key value} [array get depends] {
            if {[string match $key $target]} {
                set inputs [concat $inputs $value]
            }
        }
        if {[newer $base.obj $inputs]} {
            return
        }
        set dir [file dir $base]
        set file [file tail $base]
        puts "In $dir, compiling $file.c -> $file.obj."
        set cmd "exec cl.exe -nologo -c \
            [flag CFLAGS] [flag CFLAGS-$dir] [flag CFLAGS-$dir-$file] \
            -Fo$base.obj $base.c"
        # puts "DEBUG: $cmd"
        eval $cmd
    }

    proc link_dll {target args} {
        if {[newer $target $args]} {
            return
        }
        set dir [file dir $target]
        set file [file tail $target]
        puts "In $dir, linking $target."
        set cmd "exec link.exe -nologo -dll -machine:I386 \
            [flag LDFLAGS] [flag LDFLAGS-$dir] [flag LDFLAGS-$dir-$file] \
            -out:$target $args"
        # puts "DEBUG: $cmd"
        eval $cmd
    }

    proc link_nsd_exe {target args} {
        if {[newer $target $args]} {
            return
        }
        set dir [file dir $target]
        set file [file tail $target]
        puts "In $dir, linking $target."
        set cmd "exec link.exe -nologo -subsystem:console -machine:I386 \
            nsd.lib [flag LIB] -out:$target nsd/main.obj"
        puts "DEBUG: $cmd"
        eval $cmd
    }
}

#
# The interesting stuff starts here.
#

make::flag INCLUDE {-I"./include"}
make::flag CFLAGS [concat [make::flag INCLUDE] {-MT -W3 -GX -Zi -O2 -D "WIN32" -D "NDEBUG" -D "_WINDOWS" -D "_MBCS" -D "_USRDLL" -D "TCL_THREADS=1" -D "FD_SETSIZE=128" -D "NO_CONST=1" -YX -FD}]

make::flag LIB "-libpath:\"$::TCL_DIR/lib\" -libpath:./nsd -libpath:./nsthread"
make::flag LDFLAGS [concat [make::flag LIB] {tcl84t.lib kernel32.lib -OPT:REF}]

make::rule *.obj compile_obj
make::rule *.dll link_dll

make::rule clean {
    variable depends
    foreach file [concat \
        nsd/nsd.dll $depends(nsd/nsd.dll) \
        nsthread/nsthread.dll $depends(nsthread/nsthread.dll) \
    ] {
        puts "Deleting $file."
        file delete $file
    }
    # The semicolon is important to throw away the args from the eval.
    return;
}

make::rule install {
    variable depends
    set dir $::INSTALL_DIR

    #
    # This is what we need for AOLserver.
    #

    file mkdir $dir/bin
    file mkdir $dir/lib
    file mkdir $dir/include
    file mkdir $dir/log
    file mkdir $dir/modules/tcl
    file mkdir $dir/servers/server1/pages
    file mkdir $dir/servers/server1/modules/nslog
    file mkdir $dir/servers/server1/modules/tcl
    set files [list]
    foreach file [concat nsd/nsd.exe $depends(nsd/nsd.exe)] {
        switch -glob -- $file {
            *.exe {
                lappend files $file $dir/bin/[file tail $file]
            }
            *.dll {
                set base [file rootname $file]
                lappend files $base.dll $dir/bin/[file tail $base].dll
                lappend files $base.lib $dir/lib/[file tail $base].lib
            } 
            default {}
        }
    }
    foreach file [glob -nocomplain -directory include -type f *.h] {
        lappend files $file $dir/include/[file tail $file]
    }
    foreach file [glob -nocomplain -directory tcl -type f *.tcl] {
        lappend files $file $dir/modules/tcl/[file tail $file]
    }
    lappend files nsd/init.tcl $dir/bin/init.tcl
    lappend files sample-config.tcl $dir/sample-config.tcl

    #
    # Bring the Tcl installation over, too.
    #

    foreach file [glob -nocomplain -directory $::TCL_DIR/bin -type f *.exe] {
        lappend files $file $dir/bin/[file tail $file]
    }
    foreach file [glob -nocomplain -directory $::TCL_DIR/bin -type f *.dll] {
        lappend files $file $dir/bin/[file tail $file]
    }
    foreach file [glob -nocomplain -directory $::TCL_DIR/include -type f *.h] {
        lappend files $file $dir/include/[file tail $file]
    }
    foreach file [glob -nocomplain -directory $::TCL_DIR/lib -type f *.lib] {
        lappend files $file $dir/lib/[file tail $file]
    }
    lappend files $::TCL_DIR/lib/dde1.2 $dir/lib
    lappend files $::TCL_DIR/lib/reg1.1 $dir/lib
    lappend files $::TCL_DIR/lib/tcl8.4 $dir/lib

    #
    # Now, actually copy the files over.
    #

    foreach {src dst} $files {
        if {[file isdirectory $src] && [file exists $dst/[file tail $src]]} {
            puts "Skipping $src -> $dst.  (Already exists.)"
            continue
        }
        puts "Installing $src -> $dst."
        if {[catch {file copy -force -- $src $dst} err]} {
            set savedInfo $::errorInfo
            set savedCode $::errorCode
            if {[string match *.exe $src] || [string match *.dll $src]} {
                puts $err
            } else {
                error $err $savedInfo $savedCode
            }
        }
    }
    # The semicolon is important to throw away the args from the eval.
    return;
}

make::depends all nsd/nsd.exe
make::depends install all

make::depends nsd/nsd.exe {
    nsthread/nsthread.dll
    nsd/nsd.dll
    nssock/nssock.dll
    nscgi/nscgi.dll
    nscp/nscp.dll
    nslog/nslog.dll
    nsperm/nsperm.dll
    nsdb/nsdb.dll
    nsd/main.obj
}

make::rule nsd/nsd.exe link_nsd_exe

make::depends nsthread/nsthread.dll {
    nsthread/compat.obj
    nsthread/cslock.obj
    nsthread/error.obj
    nsthread/master.obj
    nsthread/memory.obj
    nsthread/mutex.obj
    nsthread/reentrant.obj
    nsthread/rwlock.obj
    nsthread/sema.obj
    nsthread/thread.obj
    nsthread/time.obj
    nsthread/tls.obj
    nsthread/winthread.obj
}
make::depends nsthread/*.obj include/nsthread.h
make::flag CFLAGS-nsthreads {-D "NSTHREAD_EXPORTS"}

make::depends nsd/nsd.dll {
    nsd/adpcmds.obj
    nsd/adpeval.obj
    nsd/adpparse.obj
    nsd/adprequest.obj
    nsd/auth.obj
    nsd/cache.obj
    nsd/callbacks.obj
    nsd/cls.obj
    nsd/config.obj
    nsd/conn.obj
    nsd/connio.obj
    nsd/crypt.obj
    nsd/dns.obj
    nsd/driver.obj
    nsd/dsprintf.obj
    nsd/dstring.obj
    nsd/encoding.obj
    nsd/exec.obj
    nsd/fastpath.obj
    nsd/fd.obj
    nsd/filter.obj
    nsd/form.obj
    nsd/getopt.obj
    nsd/httptime.obj
    nsd/index.obj
    nsd/info.obj
    nsd/init.obj
    nsd/lisp.obj
    nsd/listen.obj
    nsd/log.obj
    nsd/mimetypes.obj
    nsd/modload.obj
    nsd/nsconf.obj
    nsd/nsmain.obj
    nsd/nsthread.obj
    nsd/nswin32.obj
    nsd/op.obj
    nsd/pathname.obj
    nsd/pidfile.obj
    nsd/proc.obj
    nsd/queue.obj
    nsd/quotehtml.obj
    nsd/random.obj
    nsd/request.obj
    nsd/return.obj
    nsd/rollfile.obj
    nsd/sched.obj
    nsd/server.obj
    nsd/set.obj
    nsd/sock.obj
    nsd/sockcallback.obj
    nsd/stamp.obj
    nsd/str.obj
    nsd/tclatclose.obj
    nsd/tclcmds.obj
    nsd/tclconf.obj
    nsd/tclenv.obj
    nsd/tclfile.obj
    nsd/tclhttp.obj
    nsd/tclimg.obj
    nsd/tclinit.obj
    nsd/tcljob.obj
    nsd/tclmisc.obj
    nsd/tclobj.obj
    nsd/tclrequest.obj
    nsd/tclresp.obj
    nsd/tclsched.obj
    nsd/tclset.obj
    nsd/tclshare.obj
    nsd/tclsock.obj
    nsd/tclthread.obj
    nsd/tclvar.obj
    nsd/tclxkeylist.obj
    nsd/url.obj
    nsd/urlencode.obj
    nsd/urlopen.obj
    nsd/urlspace.obj
    nsd/uuencode.obj
}
make::depends nsd/*.obj {
    include/ns.h
    nsd/nsd.h
}
make::flag CFLAGS-nsd {-D "NSD_EXPORTS"}
make::flag LDFLAGS-nsd {nsthread.lib advapi32.lib ws2_32.lib}

make::flag LDFLAGS-modules-common {nsd.lib nsthread.lib ws2_32.lib -export:Ns_ModuleVersion,data -export:Ns_ModuleInit}

foreach {module objects} {
    nssock {nssock/nssock.obj}
    nscgi {nscgi/nscgi.obj}
    nscp {nscp/nscp.obj}
    nslog {nslog/nslog.obj}
    nsperm {nsperm/nsperm.obj}
    nsdb {
        nsdb/dbdrv.obj
        nsdb/dbinit.obj
        nsdb/dbtcl.obj
        nsdb/dbutil.obj
        nsdb/nsdb.obj
    }
    nsext {
        nsext/msg.obj
        nsext/nsext.obj
    }
    nspd {
        nspd/listen.obj
        nspd/log.obj
        nspd/main.obj
    }
} {
    make::depends $module/$module.dll $objects
    make::flag LDFLAGS-$module [make::flag LDFLAGS-modules-common]
}

#
# This makes the wheel turn.
#

make::build $argv

