# 由 lyrics\*.txt 生成 src\warble_table.h
#
# 元数据（# 开头，顺序随意）：
#   # name: sway
#   # title: Sway to My Beat in Cosmos
#
# 段落标记（可和别的标记/正文同行，按出现顺序生效）：
#   [cstart] ... [cend]   高潮段
#   [estart] ... [eend]   尾声
# 段落标记不输出；段内的行既属于整首，也单独组成该段。
#
# 播放动作标记（会被抽成单独的动作行，歌词里不显示）：
#   [sleep]      停顿 1 秒
#   [shortsleep] 停顿 0.5 秒
#   [next]       输出一个换行
#
# 生成规则：
#   - 一行去掉标记后的文字如果非空，先输出这一行；
#   - 同一行里的动作标记按顺序拼成 "[sleep][next]" 这样的动作行，排在文字之后；
#   - 动作行整行只由动作标记组成，C 侧据此执行，不当作歌词。
# name 缺省取文件名；title 必须写。每行只能有一句话（连续的 . ? ! 算一处）。
$ErrorActionPreference = 'Stop'

if (-not (Test-Path 'src')) { New-Item -ItemType Directory 'src' | Out-Null }

$files = @(
    'lyrics\sway-the-beat-in-cosmos.txt',
    'lyrics\if-i-can-stop-one-heart-from-breaking.txt',
    'lyrics\hope-is-the-thing-with-feathers.txt',
    'lyrics\had-i-not-seen-the-sun.txt'
)

$out = New-Object System.Collections.Generic.List[string]
$out.Add('/* 由 lyrics 目录下的歌词生成，勿手改 */')
$out.Add('')

# 把字面量转义成 C 字符串内容：反斜杠和双引号
function CStr([string]$s) {
    return $s.Replace('\', '\\').Replace('"', '\"')
}

$songs = @()
foreach ($file in $files) {
    if (-not (Test-Path $file)) { throw "gen_table: 找不到 $file" }

    $name = [System.IO.Path]::GetFileNameWithoutExtension($file)
    $title = ''
    $lines = New-Object System.Collections.Generic.List[string]
    $chorus = New-Object System.Collections.Generic.List[string]
    $ending = New-Object System.Collections.Generic.List[string]
    $inChorus = $false
    $chorusDone = $false
    $inEnding = $false
    $endingDone = $false

    $n = 0
    foreach ($line in Get-Content -LiteralPath $file -Encoding UTF8) {
        $n++
        if ($line -match '^\s*$') { continue }

        if ($line -match '^\s*#') {
            if ($line -match '^\s*#\s*name:\s*(.+?)\s*$') { $name = $Matches[1] }
            elseif ($line -match '^\s*#\s*title:\s*(.+?)\s*$') { $title = $Matches[1] }
            continue
        }

        $wasChorus = $inChorus
        $wasEnding = $inEnding
        $text = ''
        $acts = ''
        $hadSection = $false

        $i = 0
        while ($i -lt $line.Length) {
            $rest = $line.Substring($i)

            if ($rest.StartsWith('[sleep]')) {
                $acts += '[sleep]'
                $text = $text.TrimEnd()
                $i += 7
                continue
            }
            if ($rest.StartsWith('[shortsleep]')) {
                $acts += '[shortsleep]'
                $text = $text.TrimEnd()
                $i += 12
                continue
            }
            if ($rest.StartsWith('[next]')) {
                $acts += '[next]'
                $text = $text.TrimEnd()
                $i += 6
                continue
            }

            if ($rest.StartsWith('[cstart]')) {
                $hadSection = $true
                if ($inEnding) { throw "gen_table: $file 第 $n 行：高潮不能嵌在尾声里" }
                if ($inChorus -or $chorusDone) { throw "gen_table: $file 第 $n 行：一首歌只能有一段高潮" }
                $inChorus = $true
                $text = $text.TrimEnd()
                $i += 8
                continue
            }
            if ($rest.StartsWith('[cend]')) {
                $hadSection = $true
                if (-not $inChorus) { throw "gen_table: $file 第 $n 行：[cend] 前面没有 [cstart]" }
                $inChorus = $false
                $chorusDone = $true
                $text = $text.TrimEnd()
                $i += 6
                continue
            }
            if ($rest.StartsWith('[estart]')) {
                $hadSection = $true
                if ($inChorus) { throw "gen_table: $file 第 $n 行：尾声不能嵌在高潮里" }
                if ($inEnding -or $endingDone) { throw "gen_table: $file 第 $n 行：一首歌只能有一段尾声" }
                $inEnding = $true
                $text = $text.TrimEnd()
                $i += 8
                continue
            }
            if ($rest.StartsWith('[eend]')) {
                $hadSection = $true
                if (-not $inEnding) { throw "gen_table: $file 第 $n 行：[eend] 前面没有 [estart]" }
                $inEnding = $false
                $endingDone = $true
                $text = $text.TrimEnd()
                $i += 6
                continue
            }

            $text += $line[$i]
            $i++
        }

        if ($hadSection) { $text = $text.TrimStart() }

        if ($text.Trim()) {
            if (([regex]::Matches($text, '[.!?]+')).Count -gt 1) {
                throw "gen_table: $file 第 $n 行有多句话（一行只能一句）"
            }
            $lines.Add($text)
            if ($wasChorus) { $chorus.Add($text) }
            if ($wasEnding) { $ending.Add($text) }
        }

        if ($acts) {
            $lines.Add($acts)
            if ($wasChorus) { $chorus.Add($acts) }
            if ($wasEnding) { $ending.Add($acts) }
        }
    }

    if ($inChorus) { throw "gen_table: $file 的 [cstart] 没有 [cend]" }
    if ($inEnding) { throw "gen_table: $file 的 [estart] 没有 [eend]" }
    if ($lines.Count -eq 0) { throw "gen_table: $file 没有歌词" }
    if (-not $title) { throw "gen_table: $file 缺少 # title: ..." }
    if ($chorusDone -and $chorus.Count -eq 0) { throw "gen_table: $file 的高潮段是空的" }
    if ($endingDone -and $ending.Count -eq 0) { throw "gen_table: $file 的尾声是空的" }

    $sym = $name.ToUpperInvariant() -replace '[^A-Z0-9]', '_'

    $out.Add("static const char *const WARBLE_LINES_$sym[] = {")
    foreach ($line in $lines) { $out.Add('    "' + (CStr $line) + '",') }
    $out.Add('};')
    $out.Add('')

    if ($chorus.Count -gt 0) {
        $out.Add("static const char *const WARBLE_CHORUS_$sym[] = {")
        foreach ($line in $chorus) { $out.Add('    "' + (CStr $line) + '",') }
        $out.Add('};')
        $out.Add('')
    }

    if ($ending.Count -gt 0) {
        $out.Add("static const char *const WARBLE_END_$sym[] = {")
        foreach ($line in $ending) { $out.Add('    "' + (CStr $line) + '",') }
        $out.Add('};')
        $out.Add('')
    }

    $songs += [pscustomobject]@{
        Name = $name; Title = $title; Sym = $sym
        HasChorus = ($chorus.Count -gt 0); HasEnding = ($ending.Count -gt 0)
    }
}

$out.Add('static const struct warble_song WARBLE_SONGS[] = {')
foreach ($song in $songs) {
    $out.Add('    { "' + (CStr $song.Name) + '", "' + (CStr $song.Title) + '", WARBLE_LINES_' + $song.Sym + ',')
    $out.Add('      sizeof WARBLE_LINES_' + $song.Sym + ' / sizeof *WARBLE_LINES_' + $song.Sym + ',')
    if ($song.HasChorus) {
        $out.Add('      WARBLE_CHORUS_' + $song.Sym + ', sizeof WARBLE_CHORUS_' + $song.Sym + ' / sizeof *WARBLE_CHORUS_' + $song.Sym + ',')
    } else {
        $out.Add('      NULL, 0,')
    }
    if ($song.HasEnding) {
        $out.Add('      WARBLE_END_' + $song.Sym + ', sizeof WARBLE_END_' + $song.Sym + ' / sizeof *WARBLE_END_' + $song.Sym + ' },')
    } else {
        $out.Add('      NULL, 0 },')
    }
}
$out.Add('};')

$utf8 = New-Object System.Text.UTF8Encoding($false)   # 不写 BOM
[System.IO.File]::WriteAllLines((Join-Path $PWD 'src\warble_table.h'), $out, $utf8)
