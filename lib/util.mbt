const I64_MAX : Int64 = 9223372036854775807L
const I64_MIN : Int64 = -9223372036854775807L - 1L

fn float_inf(positive : Bool) -> Double {
  let zero : Double = 0.0
  let one : Double = 1.0
  if positive {
    one / zero
  } else {
    -(one / zero)
  }
}

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
    if p >= p_len {
      if star != -1 {
        p = star + 1
        star_s = star_s + 1
        s = star_s
        continue
      }
      return false
    }
    let pc = pattern[p]
    let mut elem_len = 1
    let mut is_match = false
    let mut is_star = false
    if pc == 42 {
      is_star = true
    } else if pc == 63 {
      is_match = true
    } else if pc == 91 || pc == 92 {
      let elem = parse_pattern_element(pattern, p, str[s])
      elem_len = elem.0
      is_match = elem.1
    } else {
      is_match = str[s] == pc
    }
    if is_star {
      star = p
      p = p + 1
      star_s = s
    } else if is_match {
      p = p + elem_len
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
  "\{n}"
}

fn pow10_int(n : Int) -> Double {
  let mut result = 1.0
  let mut base = 10.0
  let mut k = n
  while k > 0 {
    if k % 2 == 1 {
      result = result * base
    }
    base = base * base
    k = k / 2
  }
  result
}

pub fn parse_float(s : String) -> Double? {
  if s == "inf" || s == "+inf" {
    return Some(float_inf(true))
  }
  if s == "-inf" {
    return Some(float_inf(false))
  }
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
            if v <= 1000 {
              v = v * 10 + d
            }
            j = j + 1
          }
          None => return None
        }
      }
      let signed = if neg { -v } else { v }
      if signed > 308 || signed < -308 {
        let base_negative = base_str.length() > 0 && base_str[0] == 45
        if signed > 308 {
          return Some(if base_negative { float_inf(false) } else { float_inf(true) })
        }
        return Some(if base_negative { -0.0 } else { 0.0 })
      }
      signed
    }
    let mut result : Double = 0.0
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
    let mut divisor : Double = 1.0
    let mut base_digits = 0
    let mut base_valid = true
    let mut bi = start
    while bi < base_str.length() && base_valid {
      match uint16_to_digit(base_str[bi]) {
        Some(digit) => {
          base_digits = base_digits + 1
          let digit_f = Double::from_int(digit)
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
      result = result * pow10_int(exp_val)
    } else if exp_val < 0 {
      result = result / pow10_int(-exp_val)
    }
    if negative {
      Some(-result)
    } else {
      Some(result)
    }
  }
}

pub fn format_float(n : Double) -> String {
  if n == 0.0 {
    "0"
  } else if n != n {
    "nan"
  } else {
    let neg = n < 0.0
    let num = if neg { -n } else { n }
    if num >= 9223372036854775808.0 {
      if neg {
        "-inf"
      } else {
        "inf"
      }
    } else {
      let int_part = num.to_int64()
      let mut frac_part = num - num.floor()
      let mut result = int_part.to_string()
      if frac_part > 0.0 {
        let dot_index = result.length()
        result = result + "."
        let mut precision = 0
        while frac_part > 0.0 && precision < 15 {
          frac_part = frac_part * 10.0
          let digit = frac_part.to_int64()
          let digit_f = Double::from_int(digit.to_int())
          result = result + digit.to_string()
          frac_part = frac_part - digit_f
          precision = precision + 1
        }
        let mut end = result.length()
        while end > dot_index + 1 && result[end - 1] == 48 {
          end = end - 1
        }
        if end == dot_index + 1 {
          end = dot_index
        }
        result = result[0:end].to_owned()
      }
      if neg {
        "-" + result
      } else {
        result
      }
    }
  }
}

