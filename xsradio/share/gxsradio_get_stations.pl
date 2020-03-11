#!/usr/bin/perl -w
# This is a sample Perl script that retrieves the full list of stations from
# api.radio-browser.info, filters it according to certain criteria, and writes
# a number of .gxsradio station files in the current directory. This isn't
# supposed to be a finished piece of softare, but rather an example of how
# filter the radio-browser.info data according to personal preferences. 
#
# This script uses wget, but it would be easy enough to substitute curl, or
# just download the full stations file manually using a browser.

# My basic selection criteria -- England or English language
my $countries="United Kingdom";
my $languages="english";

# The filter() function takes three arguments: the input file (the complete
# station list in XML format from radiobrowser.info, a regular expression that
# matches tags in the station entries, and an output file, which will be in
# gxsradio format.  Matching multiple tags can be achieved by separating them
# with a vertical bar (tag1|tag2) as in any regular expression. This function
# can't filter by bit-rate, for example, but it wouldn't be hard to add that
# sort of functionality 
sub filter ($$$)
  {
  my $infile = $_[0];
  my $tags = $_[1];
  my $outfile = $_[2];

  open IN, "<$infile" or die "Can't open $infile";
  open OUT, ">$outfile" or die "Can't write $outfile";

  while (<IN>)
    {
    my $line = $_;
    if ($line) 
      {
      # Only include stations marked OK
      if ($line =~ /lastcheckok="1"/)
	{
	$line =~ /language="(.*?)"/;
	my $language = $1;
	if (!$language) { $language="***"; }
	$language =~ s/&amp;/\&/g;
	$language =~ s/&apos;/'/g;

	$line =~ /country="(.*?)"/;
	my $country = $1;
	if (!$country) { $country="***"; }
	$country =~ s/&amp;/\&/g;
	$country =~ s/&apos;/'/g;

	$line =~ /countrycode="(.*?)"/;
	my $countrycode = $1;
	if (!$countrycode) { $countrycode="?"; }

	# See if the language or country match
	if (($language =~ /$languages/) or ($country =~ /$countries/))
	  {
	  # If country or language matches, see if tags match
	  $line =~ /tags="(.*?)"/;
	  my $taglist = $1;
	  if ($taglist)
	    {
	    if ($taglist =~ /$tags/)
	      {
	      $line =~ /name="(.*?)"/;
	      my $name = $1;
	      $name =~ s/&amp;/\&/g;
	      $name =~ s/&apos;/'/g;
	      $name =~ s/&quot;/"/g;
	      $line =~ /url_resolved="(.*?)"/;
	      my $url = $1;
	      print (OUT "${name}\t${countrycode}\t${url}\t0\n");
	      }
	    }
	  } 
	}
      }
    }

  close OUT;
  close IN;
  }

# Download the full station list from here:
my $url = "http://de1.api.radio-browser.info/xml/stations";

# Keep the station list in a real (not temporary) file, in case
# we need to look at it for information about available tags, etc.
my $tempfile="/tmp/stations";

system ("wget -O $tempfile $url");

filter ($tempfile, "news|drama", "news_and_drama.gxsradio");
filter ($tempfile, "oldies", "oldies.gxsradio");
filter ($tempfile, "public radio", "public_radio.gxsradio");
filter ($tempfile, "blues", "blues.gxsradio");
filter ($tempfile, "rock", "rock.gxsradio");


