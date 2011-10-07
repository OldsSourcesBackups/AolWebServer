# requests fp_collide quickly multiple times
# correct behavior is different content every time
# incorrect behavior is same result within same second

package require http 2
set server "http://localhost:8002"
set path "/fp_collide"

proc uget {url} {
    set tok [http::geturl $url]
    set data [http::data $tok]
    http::cleanup $tok
    return $data
}

set lastval ""
for {set c 0} {$c < 10} {incr c} {
    set testval [uget $server$path]
    if {$testval eq $lastval} {
        puts "$c: collision!"
    } else {
        puts "$c: ok"
    }
    set lastval $testval
}

puts "slow gets"
for {set c 0} {$c < 10} {incr c} {
    set testval [uget $server$path]
    if {$testval eq $lastval} {
        puts "$c: collision!"
    } else {
        puts "$c: ok"
    }
    set lastval $testval
    after 500
}

