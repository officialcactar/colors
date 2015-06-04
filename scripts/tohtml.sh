#!/bin/sh

cat << EOF
<!DOCTYPE html>
<html>
<head>
<style>
.color-sample {
   float: left;
   margin: 3px;
   width: 100px;
   height: 100px;
   border-radius: 3px;
}
</style>
</head>
<body>
EOF

while read line; do
	echo "<div class=\"color-sample\" style=\"background: ${line}\"></div>"
done

cat << EOF
</body>
</html>
EOF
