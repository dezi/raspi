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

function make_nice($icy)
{
  	$icy = mb_convert_case($icy,MB_CASE_LOWER,"UTF-8");
  	$icy = preg_replace("/\\([^)]*\\)/","",$icy);
  	
  	$icy = str_replace(" - "," ^ ",$icy);
  	$icy = str_replace("-"," ",$icy);
  	$icy = str_replace(" ^ "," - ",$icy);

  	$icy = str_replace("'","",$icy);
  	$icy = str_replace("!","",$icy);
  	$icy = str_replace(",","",$icy);
  	$icy = str_replace(".","",$icy);
  	$icy = str_replace("/","",$icy);
  	$icy = str_replace("+","",$icy);
  	
  	$icy = str_replace(" and "," & ",$icy);
  	$icy = str_replace(" And "," & ",$icy);
  	
  	$icy = str_replace("ä","ae",$icy);
  	$icy = str_replace("ö","oe",$icy);
  	$icy = str_replace("ü","ue",$icy);
  	$icy = str_replace("Ä","ae",$icy);
  	$icy = str_replace("Ö","oe",$icy);
  	$icy = str_replace("Ü","ue",$icy);
  	$icy = str_replace("ß","ss",$icy);
  	
  	$icy = str_replace("  "," ",$icy);
  	$icy = str_replace("  "," ",$icy);

  	return $icy;
}

	$options = array
	(
  		'http' => array
  		(
    		'method'=>"GET",
    		'header'=>"User-Agent: KappaRadioPlaylistTracker/0.1 (dezi@kappa-mm.de) +http://www.kappa-mm.de/\r\n"
  		)
	);

	$context = stream_context_create($options);

  	$library = get_directory("./queries");
  	
  	foreach ($library as $track)
  	{
  		$trackfile = "./queries/$track";
  		$trackcont = file_get_contents($trackfile);
  		$trackjson = json_decdat($trackcont);
  		
  		if (! $trackjson) continue;
  		
  		$tracklower = make_nice(substr($track,0,-5));
  		$trackfound = null;
  		
  		$release = Array();
  		
  		//
  		// Identify exact match artists and track.
  		//
  		 
  		if (count($release) == 0)
  		{
	  		foreach ($trackjson as $entry)
  			{
  				if (! isset($entry[ "title" ])) continue;
  				
	  			if ($tracklower != make_nice($entry[ "title" ])) continue;
  				
				array_push($release,$entry);
				 
  				echo $track . "\n";
  				echo $entry[ "title" ] . "\n";
  				echo "-----\n";
  			}
  		}
  	
  		//
  		// Identify artist and track separate.
  		//
  		
  		if (count($release) == 0)
  		{
	  		foreach ($trackjson as $entry)
  			{
  				if (! isset($entry[ "title" ])) continue;
  				
  				$parts = explode(" - ",$tracklower);
  				if (count($parts) != 2) continue;
  				
  				$complower = make_nice($entry[ "title" ]);
  				
  				if (substr($complower,0,strlen($parts[ 0 ])) != $parts[ 0 ]) continue;
  				
  				if ((substr($complower, -strlen($parts[ 1 ])) != $parts[ 1 ]) &&
  					(strstr($complower," - " . $parts[ 1 ]) == false))
  				{
  					continue;
  				}
  				
				array_push($release,$entry);
				 
  				echo $track . "\n";
  				echo $entry[ "title" ] . "\n";
  				echo "-----\n";
  			}
  		}
  		
  		//
  		// Identify artist and load release tracks.
  		//
  		
  		if (count($release) == 0)
  		{
	  		foreach ($trackjson as $entry)
  			{
  				if (! isset($entry[ "title" ])) continue;
  				
  				$parts = explode(" - ",$tracklower);
  				if (count($parts) != 2) continue;
  				
  				$complower = make_nice($entry[ "title" ]);
  				
  				if (substr($complower,0,strlen($parts[ 0 ])) != $parts[ 0 ]) continue;
  								 
  				echo $track . "\n";
  				echo $entry[ "title" ] . "\n";
  				echo $entry[ "resource_url" ] . "\n";
  				echo "=====\n";
  				
  				//sleep(1);
				$releasecont = file_get_contents($entry[ "resource_url" ],false,$GLOBALS[ "context" ]);
				if (! $releasecont) exit(0);
				$releasejson = json_decdat($releasecont);
				
				if (! isset($releasejson[ "tracklist" ])) continue;
				
				foreach ($releasejson[ "tracklist" ] as $trackitem)
				{
					if (! isset($trackitem[ "title" ])) continue;

					$trackitemlower = make_nice($trackitem[ "title" ]);
					if ($trackitemlower != $parts[ 1 ]) continue;
					
					echo $track . "\n";
  					echo $trackitem[ "title" ] . "\n";
  					echo ">>>>>\n";

					array_push($release,$entry);

					break;
				}
  			}
  		}
  			
		//
		// Create release if found.
		//
	
  		if (count($release) > 0)
  		{
			$releasefile = "./releases/$track";
			file_put_contents($releasefile,json_encdat($release) . "\n");
  			unlink($trackfile);
		}
		else
		{
			if (count($trackjson) == 0)
			{
  				unlink($trackfile);
			}
			else
			{
				$schrottfile = "./schrott/$track";
	  			rename($trackfile,$schrottfile);
  			}
		}
  	}
?>
