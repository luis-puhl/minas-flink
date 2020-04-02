(find "./src" -type f -print0  | sort -z | xargs -0 sha1sum;
 find "./src" \( -type f -o -type d \) -print0 | sort -z | \
   xargs -0 stat -c '%n %a') \
| sha1sum > src-hash

# (find "./src" -type f -print0  | sort -z | xargs -0 sha1sum; find "./src" \( -type f -o -type d \) -print0 | sort -z | xargs -0 stat -c '%n %a') | sha1sum
