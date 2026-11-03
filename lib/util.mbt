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
    if p < p_len && (pattern[p] == '?' || pattern[p] == str[s]) {
      p = p + 1
      s = s + 1
    } else if p < p_len && pattern[p] == '*' {
      star = p
      p = p + 1
      star_s = s
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

