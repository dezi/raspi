<?php

include "json.php";

function build_artist($artists)
{
	$artist = "";
	
	if (isset($artists[ "artist" ]) &&
	    isset($artists[ "artist" ][ "name" ]))
	{
		$artist = $artists[ "artist" ][ "name" ];
	}
	
	if (isset($artists[ "artist" ]) &&
	    isset($artists[ "artist" ][ 0 ]) &&
	    isset($artists[ "artist" ][ 0 ]))
	{
		foreach ($artists[ "artist" ] as $name)
		{
			$artist .= " " . $name[ "name" ];
			if (isset($name[ "join" ]) && ! is_array($name[ "join" ])) $artist .= " " . $name[ "join" ];
		}
		
		$artist = trim($artist);
	}
	
	return $artist;
}

function consume_release($release)
{
	if (strlen($release) == 0) return;
	
	if (! preg_match('/<release id="([0-9]*)"/',$release,$result)) return;
	
	$id   = $result[ 1 ];
	$xml  = simplexml_load_string($release);
	$json = json_encdat($xml);
	$data = json_decdat($json);
	json_nukeempty($data);
	$json = json_encdat($data);
	$file = str_pad($id,10,"0",STR_PAD_LEFT);
	
	file_put_contents("./discogs/$file.json",trim($json) . "\n");
	
	$artist = "";
	
	if (isset($data[ "artists" ])) $artist = build_artist($data[ "artists" ]);
	
	if (isset($data[ "tracklist" ]) &&
	    isset($data[ "tracklist" ][ "track" ]))
	{
		foreach ($data[ "tracklist" ][ "track" ] as $track)
		{
			if (! isset($track[ "title" ])) continue;
			
			$title = $track[ "title" ];
			
			$thisartist = $artist;
						
			if (isset($track[ "artists" ])) $thisartist = build_artist($track[ "artists" ]);

			echo "$file $thisartist - $title\n";
		}
	}
}

	$pfd = popen("gunzip < discogs_20130401_releases.xml.gz","r");
	
	fgets($pfd);
	
	$release = "";
	
	while (($line = fgets($pfd)) != null)
	{
		if (substr($line,0,12) == "<release id=")
		{
			consume_release($release);
			$release = "";
		}
		
		$release .= $line;
	}
	
	consume_release($release);
	
	pclose($pfd);	
?>
