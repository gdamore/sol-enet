#
# ident	"%Z%%M%	%I%	%E% GED"
#
# Copyright (c) 2002-2004 by Garrett D'Amore <garrett@damore.org>.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

package XMLLite;

#
# This is just a light-weight XML parser.  It doesn't check for "conformance"
# to any DTD, nor does it parse DTD specifications.  But it does allow one
# to create cheapie perl scripts to parse (e.g. to convert) well-formed
# xml.  I use it to convert an "xml" document to HTML.

# states
#
# 0 = outside of tag
# 1 = waiting for element name
# 2 = finishing element name
# 3 = waiting for attr name
# 4 = parsing attr name
# 5 = expecting attr value
# 6 = closing special tag
# 7 = embedded comment
# 8 = parsing attr value
# 9 = parsing closing tag
# 10 = parsing xml meta tag (<? ?>) -- discarded
#
sub new {
  my $class = shift;
  my $self = {};
  bless $self,$class;
  $self->{coderefs} = {};
  return $self;
}

sub parse {
  my $self = shift;
  my $file = shift;
  my $state = 0;
  my $ch;
  my $elemname = undef;
  my $attrname = undef;
  my $attrvalue = undef;
  my $content = "";
  my %coderefs = {};
  my $ref=undef;
  my @doc = ();
  my @content = ();
  my $line = 1;

  while (defined($ch = getc($file))) {
    if ($ch eq "\n") {
	$line++;
    }
    if ($state == 0) {
      if ($ch eq '<') {
	$ref = {};
	$ref->{xml} = $self;
	$state = 1;
	$elemname = undef;
      } else {
	$content .= $ch;
      }
      next;
    }

    #
    # Expecting start of element name (excepting perhaps ! or ?)
    #
    if ($state == 1) {
      if ($ch eq '!') {
	$state = 7;
	next;
      }
      if ($ch eq '?') {
	$state = 10;
	next;
      }
      if ($ch eq '/') {
	$state = 9;
	$elemname = "";
	next;
      }
      next if ($ch =~ /\s/);
      if ($ch !~ /[\/a-zA-Z]/) {
	die "line $line: invalid character '$ch' starting element\n";
      }
      push(@content, $content);
      $content = "";
      $elemname = $ch;
      $state = 2;
      next;
    }

    #
    # Processing element name
    #
    if ($state == 2) {
      if ($ch =~ /\s/) {
	# element name terminated, probably expecting attributes
	$state = 3;
	$ref->{elemname} = $elemname;
	next;
      }

      if ($ch eq '/') {
	# end of unary element
	$state = 6;
	$ref->{elemname} = $elemname;
	$ref->{content} = undef;
	$self->process($ref);
	next;
      }

      if ($ch eq '>') {
	# end of element, push it on the list
	$ref->{elemname} = $elemname;
	$elemname = undef;
	push(@doc, $ref);
	$ref = {};
	$state = 0;
	next;
      }

      # just continuing the element name
      $elemname .= $ch;
      next;
    }

    #
    # Processing attribute list
    #
    if ($state == 3) {
      $attrname = "";
      $attrvalue = undef;

      next if $ch =~ /\s/;

      if ($ch eq '/') {
	# start closing unary element
	$state = 6;
	next;
      }

      if ($ch eq '>') {
	# closing element
	$state = 0;
	$elemname = undef;
	push(@doc, $ref);
	next;
      }

      $state = 4;
      $attrname = $ch;
      $attrvalue = undef;
      if (!defined($ref->{attrs})) {
	$ref->{attrs} = {};
      }
      next;
    }

    #
    # Processing attribute name
    #
    if ($state == 4) {
      if ($ch eq '>') {
	# end of element, also end of attribute
	$ref->{elemname} = $elemname;
	$ref->{attrs}->{$attrname} = undef;
	$elemname = undef;
	push(@doc, $ref);
	$ref = {};
	$state = 0;
	$attrname = undef;
	next;
      }

      if ($ch eq '=') {
	# end of attribute name, start looking for attr value
	$state = 5;
	$ref->{attrs}->{$attrname} = undef;
	next;
      }

      if ($ch =~ /\s/) {
	# end of attribute name, no value
	$ref->{attrs}->{$attrname} = undef;
	$attrname = undef;
	$state = 3;
	next;
      }

      $attrname .= $ch;
      next;
    }

    #
    # Start of attribute value, only quote allowed
    #
    if ($state == 5) {
      if ($ch ne '"') {
	die "line $line: attribute value not quoted\n";
      }
      $state = 8;
      next;
    }

    #
    # Special case for ">" after "/", as in "<br/>"
    #
    if ($state == 6) {
      if ($ch ne '>') {
	die "line $line: incorrect XML truncation tag\n";
      }
      $content = pop(@content);
      $ref->{contet} = $content;
      $content .= $self->process($ref);
      $state = 0;
    }

    #
    # Processing comments, we don't bother to place them on the output
    # Note that this doesn't support comments outside of the top-level!
    #
    if (($state == 7) || ($state == 10)) {
      # eat comments
      if ($ch eq '>') {
	$state = 0;
      }
      next;
    }

    #
    # Processing quoted attribute value
    #
    if ($state == 8) {
      if ($ch eq '"') {
	# done with quoted attribute value
	$ref->{attrs}->{$attrname} = $attrvalue;
	$attrname = undef;
	$attrvalue = undef;
	$state = 3;
	next;
      }
      $attrvalue .= $ch;
      next;
    }

    #
    # Processing closing tag
    #
    if ($state == 9) {
      if ($ch eq '>') {
	$ref = pop(@doc);
	if ($ref->{elemname} ne $elemname) {
	  die ("line $line: closing tag </$elemname>, expected </$ref->{elemname}>\n");
	}
	$ref->{content} = $content;
	$content = pop(@content).$self->process($ref);
	$state = 0;
	next;
      }
      $elemname .= $ch;
      next;
    }
  }

  if ($state != 0) {
    die "line $line: unclosed tag\n";
  }

  return ($content);
}

sub process {
  my $self = shift;
  my $ref = shift;
  my $elemname = $ref->{elemname};
  my $func = $self->{coderefs}->{$elemname};

  if (defined($func)) {
    return (&$func($ref));
  } else {
    return (&dumptag($ref));
  }
}

sub dumptag {
  my $ref = shift;
  my $output;
  my $key;

  $output = sprintf("<%s", $ref->{elemname});
  foreach $key (keys (%{$ref->{attrs}})) {
    if (defined ($ref->{attrs}->{$key})) {
      $output .= sprintf(" %s=\"%s\"", $key, $ref->{attrs}->{$key});
    } else {
      $output .= sprintf(" %s", $key);
    }
  }
  $output .= sprintf(">%s", $ref->{content});
  $output .= sprintf("</%s>", $ref->{elemname});
  return $output;
}

1;
