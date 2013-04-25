<?php

include "json.php";

function getchannels($content)
{
	@mkdir("./channels",0775);

	preg_match_all('|<a linkobject="{\'url\':\'http:\/\/([^.]+)\.radio\.de|',$content,$result);

	$channels = Array();
	$lastchan = "";

	foreach ($result[ 1 ] as $channel)
	{
		if ($channel == $lastchan) continue;
		if (substr($channel,0,7) == "lautfm-") continue;
		$lastchan = $channel;

		echo "$channel\n";

		$channeldir = "./channels/de.$channel";
		@mkdir($channeldir,0775);

		//
		// Fetch or load channel index page.
		//

		$channelindex = "$channeldir/de.$channel.html";

		if (file_exists($channelindex))
		{
			$channelcontent = file_get_contents($channelindex);
		}
		else
		{
			$channelcontent = file_get_contents("http://$channel.radio.de/");
			if (! $channelcontent) continue;

			file_put_contents($channelindex,$channelcontent);

			sleep(3);
		}

		//
		// Generate channel JSON file.
		//

		$alles = Array();

		preg_match_all('|var _getPlayerConfig=function\(\) \{return (.*);\}|',$channelcontent,$jsonres);
		if (! $jsonres) continue;
		$alles[ "config" ] = json_decdat($jsonres[ 1 ][ 0 ]);

		preg_match_all('|var _getPlaylist=function\(\) \{return (.*);\}|',$channelcontent,$jsonres);
		if (! $jsonres) continue;
		$alles[ "playlist" ] = json_decdat($jsonres[ 1 ][ 0 ]);

		preg_match_all('|var _getBroadcast=function\(\) \{return (.*);\}|',$channelcontent,$jsonres);
		if (! $jsonres) continue;
		$alles[ "broadcast" ] = json_decdat($jsonres[ 1 ][ 0 ]);

		$json = json_encdat($alles);

		$channeljson = "$channeldir/de.$channel.json";

		file_put_contents($channeljson,$json);
	}
}

function getlist()
{
	$listfile = "channels.www.radio.de-de.html";
	$listurl  = "http://www.radio.de/broadcast-list/broadcast-list-list.jsp"
			  . "?levels=_genre:,_language:,_topic:,_city:,_country:Deutschland"
			  . "&sorting=popular"
			  . "&selection=country"
			  . "&numberOfItems=1000"
			  ;

	if (file_exists($listfile))
	{
		return file_get_contents($listfile);
	}

	$content = file_get_contents($listurl);
	file_put_contents($listfile,$content);

	return $content;
}

$content = getlist();
getchannels($content);

?>
