import React, { useState } from 'react'
import axios from 'axios';


import './home.css'

const Home = (props) => {

    // Define save image variables
    const [image, setImage] = useState(null)
    const [result, setResult] = useState('Wait for response..');
    //setResult("Wait for response..")

    // Set image data
    const getFileInfo = (e) => {
      const reader = new FileReader()
      reader.onload = () => {
        const base64Image = reader.result
        setImage(base64Image)
      }
      reader.readAsDataURL(e.target.files[0])
    }
  
    // Post image to server
    const handleUpload = () => {
      axios.post('http://127.0.0.1:8080/image-upload', image).then(res => {
        setResult(res.data)
      })
    }


  return (
    <div className="home-container">
      <div className="home-header">
        <header
          data-thq="thq-navbar"
          className="navbarContainer home-navbar-interactive"
        >
          <span className="logo">WD-2711&apos;S IMAGE CAPTION DEMO</span>
        </header>
      </div>
      <div className="home-hero">
        <div className="heroContainer home-hero1">
          <div className="home-container1">
            <h1 className="home-hero-heading heading1">Upload Your Image</h1>
            <input type="file" onChange={getFileInfo}/>
            <button className="buttonFilled" onClick={handleUpload}>Upload</button>
          </div>
        </div>
      </div>
      <div className="home-features">
        <div className="featuresContainer">
          <div className="home-features1">
              <h1 className="home-features-heading heading1">
                The description of the image is
              </h1>
              <span className="home-features-sub-heading bodyLarge">
              <textarea value={result} className="featuresCard" readOnly />
              </span>
          </div>
        </div>
      </div>
    </div>
  )
}

export default Home
