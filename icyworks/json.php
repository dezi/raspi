#!/usr/bin/php
<?php

function json_encrec($data,$level = 0) 
{
	$pad = str_pad("\n",$level * 2 + 1,' ');
	
	switch ($type = gettype($data)) 
	{
		case 'NULL':
			return 'null';
			
		case 'boolean':
			return ($data ? 'true' : 'false');
			
		case 'integer':
		case 'double':
		case 'float':
			return $data;
			
		case 'string':
			$str = addslashes($data);
			$str = str_replace("\r","\\r",$str);
			$str = str_replace("\n","\\n",$str);
			return '"' . $str . '"';
			
		case 'object':
			$data = get_object_vars($data);
			
		case 'array':
			$output_index_count = 0;
			$output_indexed = array();
			$output_associative = array();
			
			foreach ($data as $key => $value) 
			{
				$output_indexed[] = json_encrec($value,$level + 1);
				
				$output_associative[] 
					= json_encrec($key,$level + 1) 
					. ':' 
					. json_encrec($value,$level + 1)
					;
				
				if ($output_index_count !== NULL && $output_index_count++ !== $key) 
				{
					$output_index_count = NULL;
				}
			}
			
			if ($output_index_count !== NULL) 
			{
				return "$pad" . "[" . "$pad  " . implode(",$pad  ",$output_indexed) . "$pad]";
			}
			 
			return "$pad" . "{" . "$pad  " . implode(",$pad  ",$output_associative) . "$pad}";
			
		default:
			return '';
	}
}

function json_nukeempty(&$data)
{
	if (! is_array($data)) return;
	
	foreach ($data as $key => $dummy)
	{
		if (! is_array($data[ $key ])) continue;
		
		if (count($data[ $key ])  > 0) json_nukeempty($data[ $key ]);		
		if (count($data[ $key ]) == 0) unset($data[ $key ]);
	}
} 

function json_encdat($data,$level = 0)
{
	return trim(json_encrec($data,$level));
} 

function json_decdat($data)
{
	$data = str_replace("\\'","'",$data);
	$data = str_replace("\t" ," ",$data);
	
	$result = json_decode($data,true);
	
	//if ($result === false) error_log($data);
	
	return $result;
}

?>
