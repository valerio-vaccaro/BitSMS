<?php
require_once('easybitcoin.php');

$bitcoin = new Bitcoin('username','password');

$tx = file_get_contents('php://input');

if (strlen($tx) > 0) {
    $res = $bitcoin->sendrawtransaction($tx);
    if ($res==false) {
        $res = json_decode('{"error":"malformed tx"}');
    }

} else {
    $res = json_decode('{"error":"missing tx"}');
}

echo json_encode($res);
?>