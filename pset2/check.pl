#! /usr/bin/perl

# check.pl
#    This program runs stdio versions of the tests, plus the io61
#    versions. It compares the stdio versions' output and measures
#    time and memory usage. It tries to prevent disaster: if your
#    code looks like it's generating an infinite-length file, or
#    like it's using too much memory, check.pl will kill it.
#
#    To add tests of your own, scroll down to the bottom. It should
#    be relatively clear what to do.

use Time::HiRes qw(gettimeofday);
use POSIX;
my($nkilled) = 0;
my($nerror) = 0;
my(@ratios);
my(@basetimes);
my(%fileinfo);

sub makefile ($$) {
    my($filename, $size) = @_;
    if (!-r $filename || -s $filename != $size) {
        truncate($filename, 0);
        while (-s $filename < $size) {
            system("cat /usr/share/dict/words >> $filename");
        }
        truncate($filename, $size);
    }
    $fileinfo{$filename} = [-M $filename, -C $filename, $size];
}

sub verify_file ($) {
    my($filename) = @_;
    if (exists($fileinfo{$filename})
        && ($fileinfo{$filename}->[0] != -M $filename
            || $fileinfo{$filename}->[1] != -C $filename)) {
        truncate($filename, 0);
        makefile($filename, $fileinfo{$filename}->[2]);
    }
}

sub run_time ($;$$$$) {
    my($command, $max_time, $output, $max_size, $trial) = @_;
    $max_time = 30 if !$max_time;
    my($pr, $pw) = POSIX::pipe();
    my($or, $ow) = POSIX::pipe();
    my($pid) = fork();
    if ($pid == 0) {
        setpgrp(0, 0);
        POSIX::close($pr);
        POSIX::dup2($pw, 100);
        POSIX::close($pw);
        POSIX::close($or);
        POSIX::dup2($ow, 1);
        POSIX::dup2($ow, 2);
        POSIX::close($ow);
        exec($command);
    }

    my($before) = Time::HiRes::time();
    my($died) = 0;
    1 while (waitpid(-1, WNOHANG) > 0);
    eval {
        local $SIG{"CHLD"} = sub {
            kill 9, -$pid;
            die "!";
        };
        do {
            sleep 1;
            if ($max_size && -f $output && -s $output > $max_size) {
                $died = "output file size out of control";
                kill 9, -$pid;
                die "!";
            }
        } while (Time::HiRes::time() < $before + $max_time);
        $died = sprintf("timeout after %.2fs", $max_time);
        kill 9, -$pid;
    };
    return {"error" => $died} if $died;

    my($delta) = Time::HiRes::time() - $before;

    POSIX::close($pw);
    my($nb, $buf);
    $nb = POSIX::read($pr, $buf, 2000);
    POSIX::close($pr);

    my($answer) = {};
    while (defined($nb) && $buf =~ m,\"(.*?)\"\s*:\s*([\d.]+),g) {
        $answer->{$1} = $2;
    }
    $answer->{"time"} = $delta if !defined($answer->{"time"});
    $answer->{"utime"} = $delta if !defined($answer->{"utime"});
    $answer->{"stime"} = $delta if !defined($answer->{"stime"});
    $answer->{"maxrss"} = -1 if !defined($answer->{"maxrss"});

    POSIX::close($ow);
    $buf = undef;
    $nb = POSIX::read($or, $buf, 20000);
    POSIX::close($or);
    $answer->{"stderr"} = "";
    if ($buf ne "" && defined($nb) && $nb) {
        my($tx) = "";
        foreach my $l (split(/\n/, $buf)) {
            $tx .= ($tx eq "" ? "" : "        : ") . $l . "\n" if $l ne "";
        }
        $answer->{"stderr"} = "    YOUR STDERR (TRIAL $trial): $tx" if $tx ne "";
    }

    return $answer;
}

sub run_time_median ($;$$$) {
    my($command, $max_time, $output, $max_size) = @_;
    my(@times, $etimes, $stderr, $l);
    my($tt) = 0;
    $stderr = "";

    do {
        my($t) = run_time($command, $max_time, $output, $max_size, @times + 1);
        $stderr .= $t->{"stderr"};
        if (exists($t->{"error"})) {
            return $etime if $etime;
            $etime = $t;
        } else {
            push @times, $t;
        }
        $tt += $t->{"time"};
    } while (@times < 5 && $tt < 3);

    @times = sort { $a->{"time"} <=> $b->{"time"} } @times;
    $tt = $times[int(@times / 2)];
    $tt->{"medianof"} = scalar(@times);
    $tt->{"stderr"} = $stderr;
    return $tt;
}

sub run ($$$$;$) {
    my($number, $infile, $command, $desc, $max_time) = @_;
    return if (@ARGV && !grep {
        $_ == $number
            || ($_ =~ m{^(\d+)-(\d+)$} && $number >= $1 && $number <= $2)
            || ($_ =~ m{(?:^|,)$number(,|$)})
               } @ARGV);
    verify_file($infile);
    $max_time = 30 if !$max_time;
    my($base) = $command;
    my($outsize);
    $base =~ s<(\./)([a-z]*61)><${1}stdio-$2>g;
    $base =~ s<out\.txt><baseout\.txt>g;
    print "TEST:      $number. $desc\nCOMMAND:   $command\nSTDIO:     ";
    my($t) = run_time_median($base);
    printf("%.5fs (%.5fs user, %.5fs system, %dKiB memory)\nYOUR CODE: ",
           $t->{"time"}, $t->{"utime"}, $t->{"stime"}, $t->{"maxrss"});
    if ($base =~ m<files/baseout\.txt>) {
        $outsize = (-s "files/baseout.txt") * 2;
    }
    my($tt) = run_time_median($command, 0, "files/out.txt", $outsize);
    if (defined($tt->{"error"})) {
        printf "KILLED (%s)\n", $tt->{"error"};
        ++$nkilled;
    } else {
        printf("%.5fs (%.5fs user, %.5fs system, %dKiB memory)\n",
               $tt->{"time"}, $tt->{"utime"}, $tt->{"stime"}, $tt->{"maxrss"});
        printf("RATIO:     %.2fx stdio", $t->{"time"} / $tt->{"time"});
        if ($tt->{"medianof"} != 1 && $tt->{"medianof"} == $t->{"medianof"}) {
            printf(" (median of %d trials)", $tt->{"medianof"});
        } elsif ($tt->{"medianof"} != $t->{"medianof"}) {
            printf(" (%d stdio trial%s, %d your code)", $t->{"medianof"},
                   $t->{"medianof"} == 1 ? "" : "s", $tt->{"medianof"});
        }
        printf("\n");
        push @ratios, $t->{"time"} / $tt->{"time"};
        push @basetimes, $t->{"time"};
        if ($base =~ m<files/baseout\.txt>
            && `cmp files/baseout.txt files/out.txt >/dev/null 2>&1 || echo OOPS` eq "OOPS\n") {
            print "           ERROR! files/out.txt differs from base version\n";
            ++$nerror;
        }
    }
    print $tt->{"stderr"} if $tt->{"stderr"} ne "";
    print "\n";
}

sub pl ($$) {
    my($n, $x) = @_;
    return $n . " " . ($n == 1 ? $x : $x . "s");
}

sub summary () {
    my($ntests) = @ratios + $nkilled;
    print "SUMMARY:   ", pl($ntests, "test"),
        ", $nkilled killed, ", pl($nerror, "error"), "\n";
    my($better) = scalar(grep { $_ > 1 } @ratios);
    my($worse) = scalar(grep { $_ < 1 } @ratios);
    if ($better || $worse) {
        print "           better than stdio ", pl($better, "time"),
        ", worse ", pl($worse, "time"), "\n";
    }
    my($mean, $basetime, $yourtime) = (0, 0, 0);
    for (my $i = 0; $i < @ratios; ++$i) {
        $mean += $ratios[$i];
        $basetime += $basetimes[$i];
        $yourtime += $basetimes[$i] / $ratios[$i];
    }
    if (@ratios) {
        printf "           average %.2fx stdio\n", $mean / @ratios;
        printf "           total time %.3fs stdio, %.3fs your code (%.2fx stdio)\n",
    $basetime, $yourtime, $basetime / $yourtime;
    }
}

# create some files
if (!-d "files" && (-e "files" || !mkdir("files"))) {
    print STDERR "*** Cannot run tests because 'files' cannot be created.\n";
    print STDERR "*** Remove 'files' and try again.\n";
    exit(1);
}
makefile("files/text1meg.txt", 1 << 20);
makefile("files/text5meg.txt", 5 << 20);
makefile("files/text20meg.txt", 20 << 20);

$SIG{"INT"} = sub {
    summary();
    exit(1);
};

run(1, "files/text1meg.txt",
    "./cat61 files/text1meg.txt > files/out.txt",
    "sequential regular small file 1B", 10);

run(2, "files/text1meg.txt",
    "cat files/text1meg.txt | ./cat61 | cat > files/out.txt",
    "sequential piped small file 1B", 10);

run(3, "files/text5meg.txt",
    "./cat61 files/text5meg.txt > files/out.txt",
    "sequential regular medium file 1B", 10);

run(4, "files/text5meg.txt",
    "cat files/text5meg.txt | ./cat61 | cat > files/out.txt",
    "sequential piped medium file 1B", 10);

run(5, "files/text20meg.txt",
    "./cat61 files/text20meg.txt > files/out.txt",
    "sequential regular large file 1B", 20);

run(6, "files/text20meg.txt",
    "cat files/text20meg.txt | ./cat61 | cat > files/out.txt",
    "sequential piped large file 1B", 20);

run(7, "files/text5meg.txt",
    "./blockcat61 -b 1024 files/text5meg.txt > files/out.txt",
    "sequential regular medium file 1KB", 10);

run(8, "files/text5meg.txt",
    "cat files/text5meg.txt | ./blockcat61 -b 1024 | cat > files/out.txt",
    "sequential piped medium file 1KB", 10);

run(9, "files/text20meg.txt",
    "./blockcat61 -b 1024 files/text20meg.txt > files/out.txt",
    "sequential regular large file 1KB", 20);

run(10, "files/text20meg.txt",
    "cat files/text20meg.txt | ./blockcat61 -b 1024 | cat > files/out.txt",
    "sequential piped large file 1KB", 20);

run(11, "files/text20meg.txt",
    "./blockcat61 files/text20meg.txt > files/out.txt",
    "sequential regular large file 4KB", 20);

run(12, "files/text20meg.txt",
    "cat files/text20meg.txt | ./blockcat61 | cat > files/out.txt",
    "sequential piped large file 4KB", 20);

run(13, "files/text20meg.txt",
    "./randomcat61 -b 1024 files/text20meg.txt > files/out.txt",
    "sequential regular large file random blocks 1KB", 10);

run(14, "files/text20meg.txt",
    "./randomcat61 -b 1024 -S 6582 files/text20meg.txt > files/out.txt",
    "sequential regular large file random blocks 1KB", 10);

run(15, "files/text20meg.txt",
    "./randomcat61 files/text20meg.txt > files/out.txt",
    "sequential regular large file random blocks 4KB", 10);

run(16, "files/text20meg.txt",
    "./randomcat61 -S 6582 files/text20meg.txt > files/out.txt",
    "sequential regular large file random blocks 4KB", 10);

run(17, "files/text20meg.txt",
    "./reordercat61 files/text20meg.txt > files/out.txt",
    "reordered regular large file", 20);

run(18, "files/text20meg.txt",
    "./reordercat61 -S 6582 files/text20meg.txt > files/out.txt",
    "reordered regular large file", 20);

run(19, "files/text5meg.txt",
    "./reverse61 files/text5meg.txt > files/out.txt",
    "reversed medium file", 20);

run(20, "files/text5meg.txt",
    "./stridecat61 -s 1048576 files/text5meg.txt > files/out.txt",
    "1MB stride medium file", 20);

summary();
