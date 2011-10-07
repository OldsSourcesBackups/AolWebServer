# when fastpath caches a file it uses the file inode number as the key
# and verifies that the size and modification time are the same
# if a request creates a file, returns it, and then deletes it quickly
# then 2 requests coming in within one second could collide, and incorrect
# data could be returned

nsv_set test count 0
proc fp_collide {args} {
    set filename [ns_tmpnam]
    set f [open $filename w]
    puts $f count:[nsv_incr test count]
    close $f
    ns_returnfile 200 text/plain $filename
    file delete $filename
}
ns_register_proc GET /fp_collide fp_collide

