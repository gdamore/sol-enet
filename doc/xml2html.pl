#!/usr/bin/perl

# $Id: xml2html.pl,v 1.3 2005/12/05 05:41:40 gdamore Exp $
#
# This uses the XMLLite module to convert a pseudo-xml document to HTML.
#

use strict;
use XMLLite;
use Getopt::Std;

my $xml = new XMLLite();
my $version = "unknown";

$xml->{coderefs}->{br} = \&br;
$xml->{coderefs}->{note} = \&note;
$xml->{coderefs}->{warning} = \&warning;
$xml->{coderefs}->{screen} = \&screen;
$xml->{coderefs}->{chaphead} = \&chaphead;
$xml->{coderefs}->{section} = \&section;
$xml->{coderefs}->{copyright} = \&meta;
$xml->{coderefs}->{author} = \&meta;
$xml->{coderefs}->{docnumber} = \&meta;
$xml->{coderefs}->{version} = \&version;
$xml->{coderefs}->{var} = \&var;
$xml->{coderefs}->{entry} = \&entry;
$xml->{coderefs}->{comment} = \&comment;

sub meta {
  my $ref=shift;
  return ('<meta name="'.$ref->{elemname}.'" content="'.$ref->{content}.'">');
}

sub br {
  return ("<br>");
}

sub note {
  my $ref = shift;
  return ('<blockquote><table cellspacing="3" WIDTH="100%">'.
	  '<tr><th valign="center" width="5%"><img src="note.gif"></th>'.
	  '<td width="95%">'.$ref->{content}.'</td></tr></table></blockquote>');
}

sub warning {
  my $ref = shift;
  return ('<blockquote><table cellspacing="3" WIDTH="100%">'.
	  '<tr><th valign="center" width="5%"><img src="warn.gif"></th>'.
	  '<td width="95%">'.$ref->{content}.'</td></tr></table></blockquote>');
}

sub screen {
  my $ref = shift;
  return ('<table border="0" bgcolor="#c0c0c0" width="100%"><tr><td><pre>'.
	  $ref->{content}.'</pre></td></tr></table><p></p>');
}

sub chaphead {
  my $ref = shift;
  return ('<h1 align="right">'.$ref->{content}.'</h1>');
}

sub section {
  my $ref = shift;
  return ('<h2>'.$ref->{content}.'</h2>');
}

sub entry {
  my $ref = shift;
  return ('<b><font face="Courier">'.$ref->{content}.'</font></b>');
}

sub var {
  my $ref = shift;
  return ('<var><font face="Times">'.$ref->{content}.'</font></var>');
}

sub comment {
  my $ref = shift;
  return ('<!-- '.$ref->{content}.' -->');
}

sub version {
  my $ref = shift;
  return $version;
}

my %opt;

getopts('v:', \%opt);

$version = $opt{v} if exists $opt{v};

my $fh = \*STDIN;
my $result = $xml->parse($fh);
print $result;
