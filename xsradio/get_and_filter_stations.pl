#!/usr/bin/perl -w
# This is a sample script that retrieves the full list
#  of stations from api.radio-browser.info, filters it 
#  according to certain criteria, and then writes a
#  streams.ini file that can be read by xsradio

my $outfile="streams.list.sample";
my $url = "http://de1.api.radio-browser.info/xml/stations";
my $tempfile="/tmp/stations";

system ("wget -O $tempfile $url");


my $countries="United Kingdom";
my $languages="english";
my $tags="news|drama";

open IN, "<$tempfile" or die "Can't open 'stations' file";
open OUT, ">$outfile" or die "Can't write $outfile";

while (<IN>)
  {
  my $line = $_;
  if ($line) 
    {
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

        if (($language =~ /$languages/) or ($country =~ /$countries/))
          {
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

#              print ("\"$name\" \"$tags\" \"$url\"\n");
              print (OUT "${url}____${name} (${countrycode})\n");
              }
            }

        } 
      }
    }
  }

close IN;



