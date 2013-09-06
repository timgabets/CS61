#! /usr/bin/perl

open(ACTUAL, $ARGV[0]) || die;
open(EXPECTED, $ARGV[1]) || die;
my($outname) = (@ARGV > 2 ? $ARGV[2] : $ARGV[0]);

my(@expected);
my($line, $skippable, $sort) = (0, 0, 0);
while (defined($_ = <EXPECTED>)) {
    ++$line;
    if (m{^//! \?\?\?\s+$}) {
        $skippable = 1;
    } elsif (m{^//!!SORT\s+$}) {
        $sort = 1;
    } elsif (m{^//! }) {
        s{^....(.*?)\s*$}{$1};
        my($m) = {"t" => $_, "line" => $line, "skip" => $skippable,
                  "r" => "", "match" => []};
        foreach my $x (split(/(\?\?\?|\?\?\{.*?\}(?:=\w+)?\?\?)/)) {
            if ($x eq "???") {
                $m->{r} =~ s{(?:\\ )+$}{\\s+};
                $m->{r} .= ".*";
            } elsif ($x =~ /\A\?\?\{(.*)\}=(\w+)\?\?\z/) {
                $m->{r} .= "(" . $1 . ")";
                push @{$m->{match}}, $2;
            } elsif ($x =~ /\A\?\?\{(.*)\}\?\?\z/) {
                my($contents) = $1;
                $m->{r} =~ s{(?:\\ )+$}{\\s+};
                $m->{r} .= "(?:" . $contents . ")";
            } else {
                $m->{r} .= quotemeta($x);
            }
        }
        push @expected, $m;
        $skippable = 0;
    }
}

my(@actual);
while (defined($_ = <ACTUAL>)) {
    chomp;
    push @actual, $_;
}

if ($sort) {
    @actual = sort { $a cmp $b } @actual;
}

my(%chunks);
my($a, $e) = (0, 0);
for (; $a != @actual; ++$a) {
    $_ = $actual[$a];
    if ($e == @expected && !$skippable) {
        print "$outname FAIL: Too much output (expected only ", scalar(@expected), " output lines)\n";
        print "$ARGV[0]:", $a + 1, ": Got `", $_, "`\n";
        exit(1);
    } elsif ($e == @expected) {
        next;
    }

    my($rex) = $expected[$e]->{r};
    while (my($k, $v) = each %chunks) {
        $rex =~ s{\\\?\\\?$k\\\?\\\?}{$v}g;
    }
    if (m{\A$rex\z}) {
        for (my $i = 0; $i < @{$expected[$e]->{match}}; ++$i) {
            $chunks{$expected[$e]->{match}->[$i]} = ${$i + 1};
        }
        ++$e;
    } elsif (!$expected[$e]->{skip}) {
        print "$outname FAIL: Unexpected output starting on line ", $a + 1, "\n";
        print "$ARGV[1]:", $expected[$e]->{line}, ": Expected `", $expected[$e]->{t}, "`\n";
        print "$ARGV[0]:", $a + 1, ": Got `", $_, "`\n";
        exit(1);
    }
}

if ($e != @expected) {
    print "$outname FAIL: Missing output starting on line ", scalar(@actual), "\n";
    print "$ARGV[1]:", $expected[$e]->{line}, ": Expected `", $expected[$e]->{t}, "`\n";
    exit(1);
} else {
    print "$outname OK\n";
    exit(0);
}
