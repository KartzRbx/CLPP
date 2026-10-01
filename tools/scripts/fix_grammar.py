import json
import pathlib

path = pathlib.Path("tools/vscode/syntaxes/clpp.tmLanguage.json")
text = path.read_text(encoding="utf-8")
start = text.find('"operator"')
match_key = text.find('"match": "', start)
value_start = match_key + len('"match": "')
value_end = text.find('"', value_start)
print("current", repr(text[value_start:value_end]))

regex = r"\.{3}|->|=>|\+\+|--|&&|\|\||==|!=|<=|>=|\?:|\.:|::|\.\.|\+|\-|\*|/|%|=|<|>|&|\||\^|~|\?|:|\."
encoded = json.dumps(regex)
print("encoded", encoded)
text = text[:match_key] + '"match": ' + encoded + text[value_end + 1 :]
path.write_text(text, encoding="utf-8", newline="\n")
check = path.read_text(encoding="utf-8")
again = check.find('"match": "', start)
print("written", repr(check[again + len('"match": "') : check.find('"', again + len('"match": "'))]))
