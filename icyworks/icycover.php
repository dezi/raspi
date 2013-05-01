<?php

include "json.php";

function get_directory($what)
{
	$entries = Array();
	
	$dd = opendir("./$what");
	
	while (($file = readdir($dd)) !== false)
	{
		if ($file == ".") continue;
		if ($file == "..") continue;
		
		array_push($entries,$file);
	}
	
	closedir($dd);
	
	return $entries;
}

	$options = array
	(
  		'http' => array
  		(
    		'method'=>"GET",
    		//'header'=>"User-Agent: KappaRadioPlaylistTracker/0.1 (dezi@kappa-mm.de) +http://www.kappa-mm.de/\r\n"
    		'header'=>"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.8; rv:20.0)/\r\n"
  		)
	);

	$context = stream_context_create($options);

	$songs = get_directory("./releases");
  	
  	foreach ($songs as $song)
  	{
		$songcont = file_get_contents("./releases/" . $song);
		if ($songcont === false) continue;
		$songjson = json_decdat($songcont);
		
		foreach ($songjson as $release)
		{
			if (! isset($release[ "thumb" ])) continue;
			
			$thumb = $release[ "thumb" ];
			$thumb = str_replace("R-90-","R-150-",$thumb);
			$thumb = str_replace("api.discogs.com","s.pixogs.com",$thumb);

			$parts = explode("/",$thumb);
			$local = "./images/" . $parts[ count($parts) - 1 ];
			
			if (strstr($thumb,"record90.png")) continue;
			
			if (file_exists($local)) continue;
			
			echo "$thumb => $local\n";
		
			$jpeg = file_get_contents($thumb,false,$GLOBALS[ "context" ]);
			if ($jpeg === false) continue;
			
			file_put_contents($local,$jpeg);
			sleep(1);
		}
	}
?>