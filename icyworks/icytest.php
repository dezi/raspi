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
  	
  	if (substr($icy,0,4) == "the ") $icy = substr($icy,4);
  	if (substr($icy,0,4) == "der ") $icy = substr($icy,4);
  	if (substr($icy,0,4) == "die ") $icy = substr($icy,4);
  	if (substr($icy,0,4) == "das ") $icy = substr($icy,4);
  	
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

function comp_levenshtein($str1,$str2)
{
	if ((strlen($str1) < 4) || (strlen($str2) < 4))
	{
		return ($str1 == $str2);
	}
	
	$dist = levenshtein($str1,$str2);
	$maxd = floor(min(strlen($str1),strlen($str2)) / 8);
	
	return ($dist <= $maxd);
	
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

	while (true)
	{
		$library = get_directory("./queryok");
	
		foreach ($library as $track)
		{
			$trackfile = "./queryok/$track";
			$trackcont = file_get_contents($trackfile);
		
			if ($trackcont === "")
			{
				$query = str_replace("?","",substr($track,0,-5));
				$url = "http://api.discogs.com/database/search?q=" . urlencode($query);
				$discogsjson = file_get_contents($url,false,$GLOBALS[ "context" ]);
				if ($discogsjson === false) continue;
			
				echo $trackfile . "\n";
				echo ".....\n";
			
				$discogs = json_decdat($discogsjson);
				if (! $discogs) 
				{
					echo "?????\n";
					continue;
				}
			
				$trackcont = json_encdat($discogs[ "results" ]);
				file_put_contents($trackfile,$trackcont);
			}
		
			$trackjson = json_decdat($trackcont);
		
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
				
					$complower = make_nice($entry[ "title" ]);
				
					if (! comp_levenshtein($tracklower,$complower)) continue;
				
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
				
					$complower  = make_nice($entry[ "title" ]);
					$compartist = substr($complower,0,strlen($parts[ 0 ]));
					$comptitle  = substr($complower, -strlen($parts[ 1 ]));
				
					if (! comp_levenshtein($compartist,$parts[ 0 ])) continue;
				
					if ((! comp_levenshtein($comptitle,$parts[ 1 ])) &&
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
				
					$complower  = make_nice($entry[ "title" ]);
					$compartist = explode(" - ",$complower);
					$compartist = $compartist[ 0 ];
				
					if ((! comp_levenshtein($compartist,$parts[ 0 ])) &&
						(substr($complower,0,7) != "various"))
					{
						continue;
					}
						 
					echo $track . "\n";
					echo $entry[ "title" ] . "\n";
					echo $entry[ "resource_url" ] . "\n";
					echo "=====\n";
				
					$releasecont = file_get_contents($entry[ "resource_url" ],false,$GLOBALS[ "context" ]);
					if (! $releasecont) continue;
					$releasejson = json_decdat($releasecont);
				
					if (! isset($releasejson[ "tracklist" ])) continue;
				
					foreach ($releasejson[ "tracklist" ] as $trackitem)
					{
						if (! isset($trackitem[ "title" ])) continue;

						$trackitemlower = make_nice($trackitem[ "title" ]);
					
						if (! comp_levenshtein($trackitemlower,$parts[ 1 ])) continue;
					
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
  	}
?>
