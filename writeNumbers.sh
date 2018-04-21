x=1
file=number.txt

while [ $x -le 50 ]
do
    echo "$x" >> "$file"
    x=$(($x + 1))
done