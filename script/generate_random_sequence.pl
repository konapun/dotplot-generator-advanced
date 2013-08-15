#!/usr/bin/perl
use strict;

my ($length) = @ARGV;

my $str;
my @nucs = qw(A C T G);
for my $i (1 .. $length) {
	$str .= $nucs[rand(4)];
}

print "$str\n";