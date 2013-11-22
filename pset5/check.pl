#! /usr/bin/perl -w
use Time::HiRes;

open(FOO, "sh61.c") || die "Did you delete sh61.c?";
$lines = 0;
$lines++ while defined($_ = <FOO>);
close FOO;

$rev = 'rev';

@tests = (
# Execute
    [ # 0. Test title
      # 1. Test command
      # 2. Expected test output (with newlines changed to spaces)
      'Test 1 (Simple commands)',
      'echo Hooray',
      'Hooray' ],

    [ 'Test 2',
      'echo Double Hooray',
      'Double Hooray' ],

    [ 'Test 3',
      'cat test.txt',
      'Triple Hooray',

      # 3. Setup command. This sets up the test environment, e.g. by
      #    creating input files. It's run by the normal shell, not your shell.
      'echo Triple Hooray > test.txt' ],


    [ 'Test 4 (Multi-line scripts)',
      '../sh61 -q < temp.out',
      'Line 1 Line 2 Line 3',

      'echo echo Line 1 > temp.out ; echo echo Line 2 | cat temp.out - > temp2.out ; mv -f temp2.out temp.out ; echo echo Line 3 | cat temp.out - > temp2.out ; mv -f temp2.out temp.out' ],


    [ 'Test 5 (Semicolon)',
      'echo Semi ;',
      'Semi' ],


    [ 'Test 6 (Background commands)',
      'cp a b &',
      'Copied',

      'echo Copied > a; echo Original > b',
      # 4. Cleanup command. This is run, by the normal shell, after your
      #    shell has finished.
      'sleep 0.1 && cat b' ],

    [ 'Test 7',
      'sh -c "sleep 0.1; test -r test6 && rm a" &',
      'Still here',

      'echo Still here > a; echo > test6',
      'rm test6 && sleep 0.2 && cat a' ],

    [ 'Test 8',
      'sleep 2 & ps T | grep sleep | grep -v grep | head -n 1 | wc -l',
      '1' ],

    [ 'Test 9',
      '../sh61 -q subcommand.txt & sleep 1 ; ps T | grep sleep | grep -v grep | head -n 1 | wc -l',
      'Hello 1',

      'echo "echo Hello; sleep 2" > subcommand.txt'],

    [ 'Test 10',
      '../sh61 -q subcommand.txt; ps | grep sleep | grep -v grep | head -n 1 | wc -l',
      'Hello Bye 1',

      'echo "echo Hello; sleep 2& echo Bye" > subcommand.txt'],


    [ 'Test 11 (Command lists)',
      'echo Hello ; echo There',
      'Hello There' ],

    [ 'Test 12',
      'echo Hello ; echo There;',
      'Hello There' ],

    [ 'Test 13',
      'echo Hello ;   echo There ; echo Who ; echo Are ; echo You ; echo ?',
      'Hello There Who Are You ?' ],

    [ 'Test 14',
      'rm -f temp.out ; echo Removed',
      'Removed' ],

    [ 'Test 15',
      'sh -c "sleep 0.1; echo Second" & sh -c "sleep 0.05; echo First" & sleep 0.2',
      'First Second' ],


    [ 'Test 16 (Command groups)',
      'true && echo True',
      'True' ],

    [ 'Test 17',
      'echo True || echo False',
      'True' ],

    [ 'Test 18',
      'grep -cv NotThere ../sh61.c && echo False',
      "$lines False" ],

    [ 'Test 19',
      'false || echo True',
      'True' ],

    [ 'Test 20',
      'true && false || true && echo Good',
      'Good' ],

    [ 'Test 21',
      'echo True && false || false && echo Bad',
      'True' ],


    [ 'Test 22 (Pipelines)',
      'echo Pipe | cat',
      'Pipe' ],

    [ 'Test 23',
      'echo Good | grep G',
      'Good' ],

    [ 'Test 24',
      'echo Bad | grep -c G',
      '0' ],

    [ 'Test 25',
      'echo Line | cat | wc -l',
      '1' ],

    [ 'Test 26',
      "echo GoHangASalamiImALasagnaHog | $rev | tee temp.out | $rev | $rev",
      'goHangasaLAmIimalaSAgnaHoG' ],

    [ 'Test 27',
      "$rev temp.out | $rev",
      'goHangasaLAmIimalaSAgnaHoG' ],

    [ 'Test 28',
      'cat temp.out | tr [A-Z] [a-z] | md5sum | tr -d -',
      '8e21d03f7955611616bcd2337fe9eac1' ],

    [ 'Test 29',
      "$rev temp.out | md5sum | tr [a-z] [A-Z] | tr -d -",
      '502B109B37EC769342948826736FA063' ],


    [ 'Test 30 (Redirection)',
      'echo Start ; echo File > temp.out',
      'Start File',

      '',
      'cat temp.out'],

    [ 'Test 31',
      'cat < temp.out ; echo Done',
      'File Done',

      'echo File > temp.out'],

    [ 'Test 32',
      'rm file_that_is_not_there 2> temp.out ; wc -l temp.out ; rm -f temp.out',
      '1 temp.out',

      'echo File > temp.out' ],

    [ 'Test 33',
      'sort < temp.out | ../sh61 -q subcommand.txt',
      'Bye Hello First Good',

      'echo "head -n 2 ; echo First && echo Good" > subcommand.txt; (echo Hello; echo Bye) > temp.out'],

    [ 'Test 34',
      'sort < temp.out > temp2.out ; tail -2 temp2.out ; rm -f temp.out temp2.out',
      'Bye Hello',

      # Remember -- this is a normal shell command! For your shell parentheses
      # are extra credit.
      '(echo Hello; echo Bye) > temp.out'],


    [ 'Test 35 (cd)',
      'cd / ; pwd',
      '/' ],

    [ 'Test 36',
      'cd / ; cd /usr ; pwd',
      '/usr' ],

# cd without redirecting stdout
    [ 'Test 37',
      'cd / ; cd /doesnotexist 2> /dev/null ; pwd',
      '/' ],

# Fancy conditionals
    [ 'Test 38',
      'cd / && pwd',
      '/' ],

    [ 'Test 39',
      'echo go ; cd /doesnotexist 2> /dev/null > /dev/null && pwd',
      'go' ],

    [ 'Test 40',
      'cd /doesnotexist 2> /dev/null > /dev/null || echo does not exist',
      'does not exist' ],

    [ 'Test 41',
      'cd /tmp && cd / && pwd',
      '/' ],

    [ 'Test 42',
      'cd / ; cd /doesnotexist 2> /dev/null > /dev/null ; pwd',
      '/' ],
    );

my($ntest) = 0;

my($sh) = "./sh61";
-d "out" || mkdir("out") || die "Cannot create 'out' directory\n";
my($tempfile) = "command.txt";
my($ntestfailed) = 0;

if (!-x $sh) {
    $editsh = $sh;
    $editsh =~ s,^\./,,;
    print STDERR "$sh does not exist, so I can't run any tests!\n(Try running \"make $editsh\" to create $sh.)\n";
    exit(1);
}

select STDOUT;
$| = 1;

sub run ($) {
    my($desc, $in, $want, $setup, $teardown) = @$test;
    if ($desc =~ /^Test (\d+)/) {
        my($number) = $1;
        return if (@ARGV && !grep {
            $_ eq $number
                || ($_ =~ m{^(\d+)-(\d+)$} && $number >= $1 && $number <= $2)
                || ($_ =~ m{(?:^|,)$number(,|$)})
                   } @ARGV);
    }

    $ntest++;
    system("cd out; $setup") if $setup;
    print "Running $desc...";
    open(F, ">out/$tempfile") || die;
    print F $in, "\n";
    close(F);
    my($start) = Time::HiRes::time();
    system("cd out; ../$sh -q < $tempfile > out.txt 2>&1");
    my($delta) = sprintf("%g", Time::HiRes::time() - $start);
    $result = `cat out/out.txt`;
    $result .= `cd out; $teardown` if $teardown;
    print STDOUT " done\n";
    $result =~ s%^sh61[\[\]\d]*\$ %%;
    $result =~ s%sh61[\[\]\d]*\$ $%%;
    $result =~ s%^\[\d+\]\s+\d+$%%mg;
    $result =~ s|\[\d+\]||g;
    $result =~ s|^\s+||g;
    $result =~ s|\s+| |g;
    $result =~ s|\s+$||;
    return if $result eq $want;
    return if $want eq 'Syntax error [NULL]' && $result eq '[NULL]';
    return if $result eq $want;
    print "$desc FAILED in $delta sec!\n  input was \"$in\"\n  expected output like \"$want\"\n  got \"$result\"\n";
    $ntestfailed += 1;
}

foreach $test (@tests) {
    run($test);
}

my($ntestpassed) = $ntest - $ntestfailed;
print "$ntestpassed of $ntest tests passed\n";
exit(0);
