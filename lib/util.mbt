const I64_MAX : Int64 = 9223372036854775807L
const I64_MIN : Int64 = -9223372036854775807L - 1L

pub fn match_pattern(pattern : String, str : String) -> Bool {
  match_pattern_helper(pattern, str, 0, 0)
}

pub fn match_pattern_helper(pattern : String, str : String, p_idx : Int, s_idx : Int) -> Bool {
  let p_len = pattern.length()
  let s_len = str.length()
  let mut p = p_idx
  let mut s = s_idx
  let mut star = -1
  let mut star_s = 0
  while s < s_len {
    let elem = parse_pattern_element(pattern, p, str[s])
    if elem.2 {
      star = p
      p = p + 1
      star_s = s
    } else if elem.1 {
      p = p + elem.0
      s = s + 1
    } else if star != -1 {
      p = star + 1
      star_s = star_s + 1
      s = star_s
    } else {
      return false
    }
  }
  while p < p_len && pattern[p] == '*' {
    p = p + 1
  }
  p == p_len
}

fn parse_pattern_element(pattern : String, p : Int, ch : UInt16) -> (Int, Bool, Bool) {
  let p_len = pattern.length()
  if p >= p_len {
    return (0, false, false)
  }
  let pc = pattern[p]
  if pc == 42 {
    return (1, false, true)
  }
  if pc == 63 {
    return (1, true, false)
  }
  if pc == 92 && p + 1 < p_len {
    return (2, pattern[p + 1] == ch, false)
  }
  if pc == 91 {
    let mut j = p + 1
    let mut negated = false
    if j < p_len && pattern[j] == 94 {
      negated = true
      j = j + 1
    }
    let mut matched = false
    let mut closed = false
    while j < p_len {
      if pattern[j] == 93 {
        closed = true
        break
      }
      let c_start = pattern[j]
      if c_start == 92 && j + 1 < p_len && pattern[j + 1] != 93 {
        if ch == pattern[j + 1] {
          matched = true
        }
        j = j + 2
      } else if j + 2 < p_len && pattern[j + 1] == 45 && pattern[j + 2] != 93 {
        if ch >= c_start && ch <= pattern[j + 2] {
          matched = true
        }
        j = j + 3
      } else {
        if ch == c_start {
          matched = true
        }
        j = j + 1
      }
    }
    if !closed {
      return (1, pc == ch, false)
    }
    let ok = if negated { !matched } else { matched }
    return (j + 1 - p, ok, false)
  }
  (1, pc == ch, false)
}

pub fn uint16_to_digit(c : UInt16) -> Int? {
  match c {
    48 => Some(0)
    49 => Some(1)
    50 => Some(2)
    51 => Some(3)
    52 => Some(4)
    53 => Some(5)
    54 => Some(6)
    55 => Some(7)
    56 => Some(8)
    57 => Some(9)
    _ => None
  }
}

pub fn digit_to_uint16(n : Int) -> UInt16 {
  match n {
    0 => 48
    1 => 49
    2 => 50
    3 => 51
    4 => 52
    5 => 53
    6 => 54
    7 => 55
    8 => 56
    9 => 57
    _ => 48
  }
}

fn checked_add_i64(a : Int64, b : Int64) -> Int64? {
  let sum = a + b
  if (b > 0 && sum < a) || (b < 0 && sum > a) {
    None
  } else {
    Some(sum)
  }
}

pub fn parse_int(s : String) -> Int? {
  match parse_int64(s) {
    Some(n) =>
      if n >= -2147483648L && n <= 2147483647L {
        Some(n.to_int())
      } else {
        None
      }
    None => None
  }
}

pub fn parse_int64(s : String) -> Int64? {
  if s.length() == 0 {
    None
  } else {
    let mut result : Int64 = 0
    let mut negative = false
    let mut start = 0
    if s[0] == 45 {
      negative = true
      start = 1
    } else if s[0] == 43 {
      start = 1
    }
    if start >= s.length() {
      return None
    }
    let max_div10 : Int64 = 922337203685477580
    for i = start; i < s.length(); i = i + 1 {
      match uint16_to_digit(s[i]) {
        Some(digit) => {
          let d = digit.to_int64()
          if result > max_div10 {
            return None
          }
          if result == max_div10 {
            let limit : Int64 = if negative { 8 } else { 7 }
            if d > limit {
              return None
            }
          }
          result = result * 10 + d
        }
        None => return None
      }
    }
    if negative {
      if result < 0 {
        Some(result)
      } else {
        Some(-result)
      }
    } else {
      Some(result)
    }
  }
}

pub fn int_to_string(n : Int) -> String {
  if n == 0 {
    "0"
  } else {
    let mut num = n
    let chars : Array[String] = []
    let mut is_negative = false
    if num < 0 {
      is_negative = true
    }
    while num != 0 {
      let digit = num % 10
      if is_negative {
        match -digit {
          0 => chars.push("0")
          1 => chars.push("1")
          2 => chars.push("2")
          3 => chars.push("3")
          4 => chars.push("4")
          5 => chars.push("5")
          6 => chars.push("6")
          7 => chars.push("7")
          8 => chars.push("8")
          9 => chars.push("9")
          _ => chars.push("0")
        }
      } else {
        match digit {
          0 => chars.push("0")
          1 => chars.push("1")
          2 => chars.push("2")
          3 => chars.push("3")
          4 => chars.push("4")
          5 => chars.push("5")
          6 => chars.push("6")
          7 => chars.push("7")
          8 => chars.push("8")
          9 => chars.push("9")
          _ => chars.push("0")
        }
      }
      num = num / 10
    }
    if is_negative {
      chars.push("-")
    }
    let result : Array[String] = []
    for i = chars.length() - 1; i >= 0; i = i - 1 {
      result.push(chars[i])
    }
    result.join("")
  }
}

pub fn parse_float(s : String) -> Float? {
  if s.length() == 0 {
    None
  } else {
    let mut e_pos = -1
    let mut i = 0
    while i < s.length() {
      if s[i] == 101 || s[i] == 69 {
        if e_pos != -1 {
          return None
        }
        e_pos = i
      }
      i = i + 1
    }
    let base_str = if e_pos == -1 { s } else { s[0:e_pos].to_owned() }
    let exp_val : Int = if e_pos == -1 {
      0
    } else {
      let exp_str = s[e_pos + 1:s.length()].to_owned()
      if exp_str.length() == 0 {
        return None
      }
      let mut neg = false
      let mut j = 0
      if exp_str[0] == 45 {
        neg = true
        j = 1
      } else if exp_str[0] == 43 {
        j = 1
      }
      if j >= exp_str.length() {
        return None
      }
      let mut v = 0
      while j < exp_str.length() {
        match uint16_to_digit(exp_str[j]) {
          Some(d) => {
            v = v * 10 + d
            j = j + 1
          }
          None => return None
        }
      }
      let signed = if neg { -v } else { v }
      if signed > 60 || signed < -60 {
        return None
      }
      signed
    }
    let mut result : Float = 0.0
    let mut negative = false
    let mut start = 0
    if base_str.length() == 0 {
      return None
    }
    if base_str[0] == 45 {
      negative = true
      start = 1
    } else if base_str[0] == 43 {
      start = 1
    }
    if start >= base_str.length() {
      return None
    }
    let mut has_dot = false
    let mut divisor : Float = 1.0
    let mut base_digits = 0
    let mut base_valid = true
    let mut bi = start
    while bi < base_str.length() && base_valid {
      match uint16_to_digit(base_str[bi]) {
        Some(digit) => {
          base_digits = base_digits + 1
          let digit_f = Float::from_int(digit)
          if has_dot {
            divisor = divisor * 10.0
            result = result + digit_f / divisor
          } else {
            result = result * 10.0 + digit_f
          }
        }
        None => {
          if base_str[bi] == 46 && !has_dot {
            has_dot = true
          } else {
            base_valid = false
          }
        }
      }
      bi = bi + 1
    }
    if !base_valid || base_digits == 0 {
      return None
    }
    if exp_val > 0 {
      let mut k = 0
      while k < exp_val {
        result = result * 10.0
        k = k + 1
      }
    } else if exp_val < 0 {
      let limit = -exp_val
      let mut k = 0
      while k < limit {
        result = result / 10.0
        k = k + 1
      }
    }
    if negative {
      Some(-result)
    } else {
      Some(result)
    }
  }
}

pub fn format_float(n : Float) -> String {
  if n == 0.0 {
    "0"
  } else {
    let mut num = n
    if num < 0.0 {
      num = -num
    }
    let int_part = num.to_int()
    let int_part_f = Float::from_int(int_part)
    let mut frac_part = num - int_part_f
    let mut result = int_to_string(int_part)
    if frac_part > 0.0 {
      result = result + "."
      let mut precision = 0
      while frac_part > 0.0 && precision < 10 {
        frac_part = frac_part * 10.0
        let digit = frac_part.to_int()
        let digit_f = Float::from_int(digit)
        result = result + digit_to_string(digit)
        frac_part = frac_part - digit_f
        precision = precision + 1
      }
    }
    if n < 0.0 {
      "-" + result
    } else {
      result
    }
  }
}

fn digit_to_string(n : Int) -> String {
  match n {
    0 => "0"
    1 => "1"
    2 => "2"
    3 => "3"
    4 => "4"
    5 => "5"
    6 => "6"
    7 => "7"
    8 => "8"
    9 => "9"
    _ => "0"
  }
}

