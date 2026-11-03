pub struct Deque {
  mut front : Array[String]
  mut back : Array[String]
}

pub fn Deque::new() -> Deque {
  Deque::{ front: [], back: [] }
}

pub fn Deque::push_front(self : Deque, value : String) -> Unit {
  self.front.push(value)
}

pub fn Deque::push_back(self : Deque, value : String) -> Unit {
  self.back.push(value)
}

pub fn Deque::pop_front(self : Deque) -> String? {
  if self.front.length() == 0 {
    while self.back.length() > 0 {
      let item = self.back.pop()
      match item {
        Some(v) => self.front.push(v)
        None => break
      }
    }
  }
  self.front.pop()
}

pub fn Deque::pop_back(self : Deque) -> String? {
  if self.back.length() == 0 {
    while self.front.length() > 0 {
      let item = self.front.pop()
      match item {
        Some(v) => self.back.push(v)
        None => break
      }
    }
  }
  self.back.pop()
}

pub fn Deque::length(self : Deque) -> Int {
  self.front.length() + self.back.length()
}

pub fn Deque::to_array(self : Deque) -> Array[String] {
  let result = []
  for i = self.front.length() - 1; i >= 0; i = i - 1 {
    result.push(self.front[i])
  }
  for item in self.back {
    result.push(item)
  }
  result
}

pub fn Deque::get_at(self : Deque, index : Int) -> String? {
  let len = self.length()
  if index < 0 || index >= len {
    None
  } else {
    let front_len = self.front.length()
    if index < front_len {
      Some(self.front[front_len - 1 - index])
    } else {
      Some(self.back[index - front_len])
    }
  }
}

pub fn Deque::set_at(self : Deque, index : Int, value : String) -> Bool {
  let len = self.length()
  if index < 0 || index >= len {
    false
  } else {
    let front_len = self.front.length()
    if index < front_len {
      self.front[front_len - 1 - index] = value
    } else {
      self.back[index - front_len] = value
    }
    true
  }
}

pub fn Deque::remove_at(self : Deque, index : Int) -> Bool {
  let len = self.length()
  if index < 0 || index >= len {
    false
  } else {
    let front_len = self.front.length()
    if index < front_len {
      let idx = front_len - 1 - index
      let new_front : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_front.push(self.front[i])
      }
      for i = idx + 1; i < front_len; i = i + 1 {
        new_front.push(self.front[i])
      }
      self.front = new_front
    } else {
      let idx = index - front_len
      let new_back : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_back.push(self.back[i])
      }
      for i = idx + 1; i < self.back.length(); i = i + 1 {
        new_back.push(self.back[i])
      }
      self.back = new_back
    }
    true
  }
}

pub fn Deque::find_index(self : Deque, value : String) -> Int {
  let arr = self.to_array()
  for i = 0; i < arr.length(); i = i + 1 {
    if arr[i] == value {
      return i
    }
  }
  -1
}

pub fn Deque::insert_at(self : Deque, index : Int, value : String) -> Bool {
  let len = self.length()
  if index < 0 || index > len {
    false
  } else {
    let front_len = self.front.length()
    if index <= front_len {
      let idx = front_len - index
      let new_front : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_front.push(self.front[i])
      }
      new_front.push(value)
      for i = idx; i < front_len; i = i + 1 {
        new_front.push(self.front[i])
      }
      self.front = new_front
    } else {
      let idx = index - front_len
      let new_back : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_back.push(self.back[i])
      }
      new_back.push(value)
      for i = idx; i < self.back.length(); i = i + 1 {
        new_back.push(self.back[i])
      }
      self.back = new_back
    }
    true
  }
}

